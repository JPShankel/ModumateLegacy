// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/Terrain.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2DVertex.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/DynamicTerrainActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UI/RightMenu/GeneralListItemMenuBlock.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Objects/TerrainMaterial.h"
#include "Objects/ModumateObjectStatics.h"


AMOITerrain::AMOITerrain()
{
	FWebMOIProperty prop;

	prop.name = TEXT("Xray");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("X-Ray");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("Visible");
	prop.type = EWebMOIPropertyType::boolean;
	prop.displayName = TEXT("Visible");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);
}

FVector AMOITerrain::GetCorner(int32 index) const
{
	return FVector::ZeroVector;
}

int32 AMOITerrain::GetNumCorners() const
{
	return InstanceData.Heights.Num();
}

FVector AMOITerrain::GetNormal() const
{
	return FVector::UpVector;
}

FVector AMOITerrain::GetLocation() const
{
	return InstanceData.Origin;
}

FQuat AMOITerrain::GetRotation() const
{
	return FQuat::Identity;
}

bool AMOITerrain::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		return UpdateTerrainActor();

	case EObjectDirtyFlags::Visuals:
		UpdateSiteMaterials();
		return TryUpdateVisuals();

	default:
		break;
	}

	return true;
}

bool AMOITerrain::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	if (!Super::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled))
	{
		return false;
	}

	if (bOutVisible)
	{
		bOutCollisionEnabled = !bIsTranslucent;
	}

	auto* actor = GetActor();
	actor->SetActorHiddenInGame(!bOutVisible);
	actor->SetActorEnableCollision(bOutCollisionEnabled);
	actor->SetActorTransform(FTransform(FQuat::Identity, GetLocation()) );

	return true;
}

void AMOITerrain::GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	const ADynamicTerrainActor* actor = Cast<ADynamicTerrainActor>(GetActor());

	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (bGetFarLines)
	{
		actor->GetFarDraftingLines(ParentPage, Plane, BoundingBox);
	}
	else
	{
		actor->GetCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox);
	}
}

void AMOITerrain::GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines,
	bool bForSnapping /*= false*/, bool bForSelection /*= false*/) const
{
	const auto graph2d = GetDocument()->FindSurfaceGraph(ID);

	if (ensure(graph2d))
	{
		const auto& polygons = graph2d->GetPolygons();

		for (const auto& polyKvp: polygons)
		{
			if (!polyKvp.Value.bInterior && polyKvp.Value.ContainingPolyID == MOD_ID_NONE)
			{
				const auto& poly = polyKvp.Value;
				for (const auto& edgeID : poly.Edges)
				{
					const FGraph2DEdge* edge = graph2d->FindEdge(edgeID);
					if (edge == nullptr)
					{
						continue;
					}
					auto startVert = graph2d->FindVertex(edge->StartVertexID);
					auto endVert = graph2d->FindVertex(edge->StartVertexID);

					if (ensure(startVert && endVert && InstanceData.Heights.Contains(edge->StartVertexID) && InstanceData.Heights.Contains(edge->EndVertexID)))
					{
						FVector pointA(GraphToWorldPosition(graph2d->FindVertex(edge->StartVertexID)->Position, InstanceData.Heights[edge->StartVertexID]));
						FVector pointB(GraphToWorldPosition(graph2d->FindVertex(edge->EndVertexID)->Position, InstanceData.Heights[edge->EndVertexID]));

						outPoints.Emplace(pointA, (pointB - pointA).GetSafeNormal(), edge->StartVertexID);
						outLines.Emplace(pointA, pointB, edge->StartVertexID, edge->EndVertexID);
					}
				}
			}
		}
	}
}

void AMOITerrain::SetIsTranslucent(bool NewIsTranslucent)
{
	bIsTranslucent = NewIsTranslucent;
	UpdateSiteMaterials(true);
	MarkDirty(EObjectDirtyFlags::Visuals);

	UpdateEditTerrainList();
}

void AMOITerrain::PostCreateObject(bool bNewObject)
{
	Super::PostCreateObject(bNewObject);

	UpdateEditTerrainList();
}

void AMOITerrain::PreDestroy()
{
	Super::PreDestroy();

	bDestroyed = true;

	UpdateEditTerrainList();
}

bool AMOITerrain::OnSelected(bool bIsSelected)
{
	if (!Super::OnSelected(bIsSelected))
	{
		return false;
	}

	auto* actor = Cast<ADynamicTerrainActor>(GetActor());
	if (!actor || !actor->Mesh)
	{
		return true;
	}

	return true;
}

AActor* AMOITerrain::CreateActor(const FVector& loc, const FQuat& rot)
{
	ADynamicTerrainActor* actor =
		GetWorld()->SpawnActor<ADynamicTerrainActor>(ADynamicTerrainActor::StaticClass(), FTransform(rot, loc));
	if (!ensure(actor))
	{
		return nullptr;
	}

	actor->Mesh->SetCollisionObjectType(UModumateTypeStatics::CollisionTypeFromObjectType(GetObjectType()));
	return actor;
}

bool AMOITerrain::GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const
{
	OutState = GetStateData();
	FMOITerrainData newData = InstanceData;
	newData.Origin = Transform.GetLocation();
	return OutState.CustomData.SaveStructData(newData);
}

bool AMOITerrain::UpdateTerrainActor()
{
	// Don't evaluate terrain mesh on server as it's costly and not required.
#if !UE_SERVER

	const auto graph2d = GetDocument()->FindSurfaceGraph(ID);

	if (ensure(graph2d))
	{
		if (graph2d->IsDirty())
		{
			return false;
		}

		const auto& polygons = graph2d->GetPolygons();
		ADynamicTerrainActor* actor = Cast<ADynamicTerrainActor>(GetActor());
		if (!ensure(actor))
		{
			return true;
		}
		actor->ClearAllMeshSections();
		PolyIDToMeshSection.Empty();

		TArray<int32> islandPolys;
		for (const auto& polygonKvp: polygons)
		{
			if (!polygonKvp.Value.bInterior && polygonKvp.Value.ContainingPolyID == MOD_ID_NONE)
			{
				islandPolys.Add(polygonKvp.Key);
			}
		}

		int32 meshSectionNumber = 0;
		for (int32 island: islandPolys)
		{
			const FGraph2DPolygon* polygon = graph2d->FindPolygon(island);

			TArray<FVector> heightPoints;
			TSet<int32> vertexIDs;
			TSet<int32> containedPolys;

			TFunction<void(int32)> addVertexHeights = [&heightPoints, &vertexIDs, &containedPolys, graph2d, this, &addVertexHeights](int32 Polygon)
			{
				const FGraph2DPolygon* polygon2d = graph2d->FindPolygon(Polygon);
				if (polygon2d->bInterior)
				{
					containedPolys.Add(Polygon);
				}

				for (int32 v: polygon2d->VertexIDs)
				{
					if (!vertexIDs.Contains(v) && InstanceData.Heights.Contains(v) && graph2d->GetVertices().Contains(v))
					{
						FVector2D graph2dPosition = graph2d->GetVertices()[v].Position;
						heightPoints.Add(GraphToWorldPosition(graph2dPosition, InstanceData.Heights[v], true));
						vertexIDs.Add(v);
					}
				}

				// Add all contained polys:
				for (int32 subPoly: polygon2d->ContainedPolyIDs)
				{
					if (!containedPolys.Contains(subPoly))
					{
						addVertexHeights(subPoly);
					}
				}

			};

			TSet<int32> interiorPolys;
			for (int32 edge: polygon->Edges)
			{
				interiorPolys.Add(graph2d->FindEdge(edge)->LeftPolyID);
				interiorPolys.Add(graph2d->FindEdge(edge)->RightPolyID);
			}
			for (int32 poly: interiorPolys)
			{
				if (poly != MOD_ID_NONE)
				{
					addVertexHeights(poly);
				}
			}

			// Calculate target grid size per island.
			static constexpr float verticesDensityPerRow = 30.0f;
			static constexpr float minGridSize = 25.0f;
			static constexpr float maxGridSize = 1000.0f;
			float gridSize = maxGridSize;
			for (int32 containedPoly: containedPolys)
			{
				const FGraph2DPolygon* containedPolygon = graph2d->FindPolygon(containedPoly);
				FBox polygonBox3D(ForceInitToZero);
				for (int32 v: containedPolygon->VertexIDs)
				{
					if (graph2d->GetVertices().Contains(v) && InstanceData.Heights.Contains(v))
					{
						polygonBox3D += FVector(graph2d->GetVertices()[v].Position, InstanceData.Heights[v]);
					}
				}
				const float Area = polygonBox3D.GetSize().X * polygonBox3D.GetSize().Y;
				const FVector size = polygonBox3D.GetSize();
				float density = 0.1f * size.Z;
				float targetGridSize = FMath::Max(size.X, size.Y) / density;
				gridSize = FMath::Min(FMath::Clamp(targetGridSize, minGridSize, maxGridSize), gridSize);
			}

			for (int32 containedPoly: containedPolys)
			{
				const FGraph2DPolygon* containedPolygon = graph2d->FindPolygon(containedPoly);
				
				TArray<FPolyHole2D> holes;
				// Inner polys become holes:
				for (int32 otherPolyID : containedPolygon->ContainedPolyIDs)
				{
					const FGraph2DPolygon* otherPolygon = graph2d->FindPolygon(otherPolyID);
					TArray<FVector2D> holePoints(otherPolygon->CachedPerimeterPoints);
					if (holePoints.Num() != 0)
					{
						holes.Emplace(holePoints);
					}
				}

				if (containedPolygon->CachedPerimeterPoints.Num() >= 3)
				{
					actor->SetupTerrainGeometry(meshSectionNumber, gridSize, containedPolygon->CachedPerimeterPoints, heightPoints, holes, true, true);
					PolyIDToMeshSection.Add(containedPoly, meshSectionNumber);
					++meshSectionNumber;
				}

			}

		}
	}

	Document->DirtyAllCutPlanes();
#endif
	return true;
}

void AMOITerrain::UpdateSiteMaterials(bool bForceUpdate/* = false*/)
{
#if !UE_SERVER
	const auto* doc = GetDocument();
	const auto graph2d = doc->FindSurfaceGraph(ID);

	if (ensure(graph2d))
	{
		auto* actor = Cast<ADynamicTerrainActor>(GetActor());
		if (!actor || !actor->Mesh)
		{
			return;
		}
		UProceduralMeshComponent* mesh = actor->Mesh;

		const auto& faces = graph2d->GetPolygons();
		for (const auto& faceKvp: faces)
		{
			const AModumateObjectInstance* faceMoi = doc->GetObjectById(faceKvp.Key);
			if (ensure(faceMoi) && faceMoi->GetChildIDs().Num() == 1)
			{
				const AMOITerrainMaterial* materialMoi = Cast<AMOITerrainMaterial>(doc->GetObjectById(faceMoi->GetChildIDs()[0]) );
				FGuid materialGuid = materialMoi->InstanceData.Material;

				// TODO: Use a terrain assembly spec rather than presets directly, when they are available.
				const FBIMPresetCollection& presets = doc->GetPresetCollection();
				const FBIMPresetInstance* preset = presets.PresetFromGUID(materialGuid);
				FBIMPresetMaterialBindingSet materialBinding;
				if (preset && preset->TryGetCustomData(materialBinding))
				{
					const int32* meshSectionPointer = PolyIDToMeshSection.Find(faceKvp.Key);
					if (meshSectionPointer)
					{
						int32 meshSectionIndex = *meshSectionPointer;
						if (CachedMaterials.FindOrAdd(meshSectionIndex) != materialGuid || bForceUpdate)
						{
							FGuid innerMaterialGuid = materialBinding.MaterialBindings[0].InnerMaterialGUID;
							FColor tint(FColor::FromHex(materialBinding.MaterialBindings[0].ColorHexValue));
							static const FName colorMultiplierName(TEXT("colorMultiplier"));

							UModumateGameInstance* gameInstance = GetWorld()->GetGameInstance<UModumateGameInstance>();
							const FArchitecturalMaterial* material = doc->GetPresetCollection().GetArchitecturalMaterialByGUID(innerMaterialGuid);
							if (material)
							{
								// TODO: Create material only for new mesh section, changing material for exiting section only needs to copy material params
								auto* gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
								if (gameMode)
								{
									UMaterialInterface* gmTerrainMat = bIsTranslucent ? gameMode->TerrainMaterialTranslucent : gameMode->TerrainMaterial;
									UMaterialInstanceDynamic* materialInst = UMaterialInstanceDynamic::Create(gmTerrainMat, mesh);
									materialInst->CopyMaterialUniformParameters(material->EngineMaterial.Get());
									materialInst->SetVectorParameterValue(colorMultiplierName, tint);
									static const FName opacityParamName(TEXT("OpacityValue"));
									//if the opacity value of the base material is 0 we should not override this value
									float opacityValue;
									materialInst->GetScalarParameterValue(opacityParamName, opacityValue);
									if (opacityValue > SMALL_NUMBER)
									{
										materialInst->SetScalarParameterValue(opacityParamName, bIsTranslucent ? 0.5f : 1.f);
									}

									mesh->SetMaterial(meshSectionIndex, materialInst);
									CachedMaterials[meshSectionIndex] = materialGuid;
								}
							}
						}
					}
				}
			}
		}
	}

#endif
}

void AMOITerrain::UpdateEditTerrainList()
{
	UWorld* world = GetWorld();
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->ToolTrayWidget->ToolTrayBlockTerrainList->UpdateAsTerrainList();
	}
}

FVector AMOITerrain::GraphToWorldPosition(FVector2D GraphPos, double Height /*= 0.0*/, bool bRelative /*= false*/) const
{
	return (bRelative ? FVector::ZeroVector : InstanceData.Origin) + GraphPos.X * FVector::ForwardVector + GraphPos.Y * FVector::RightVector
		+ Height * FVector::UpVector;
}

bool AMOITerrain::FromWebMOI(const FString& InJson)
{
	FWebMOI webMOI;
	if (!AModumateObjectInstance::FromWebMOI(InJson))
	{
		return false;
	}

	if (!ReadJsonGeneric<FWebMOI>(InJson, &webMOI))
	{
		return false;
	}

	const FWebMOIProperty* formPropXray = webMOI.properties.Find(TEXT("Xray"));
	bool bXray = formPropXray->valueArray[0] == TEXT("true");
	if (bXray != GetIsTranslucent()) {
		SetIsTranslucent(bXray);
	}

	const FWebMOIProperty* formPropVisible = webMOI.properties.Find(TEXT("Visible"));
	bool bLocalVisible = formPropVisible->valueArray[0] == TEXT("true");
	if (bLocalVisible == HideRequests.Contains(FName(TEXT("WebPropertiesPanel")))) {
		RequestHidden(FName(TEXT("WebPropertiesPanel")), !bLocalVisible);
	}

	return false;
}

bool AMOITerrain::ToWebMOI(FWebMOI& OutMOI) const
{
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		const FWebMOIProperty* formPropXray = WebProperties.Find(TEXT("Xray"));
		FWebMOIProperty webPropXray = *formPropXray;
		FString translucent = GetIsTranslucent() ? TEXT("true") : TEXT("false");
		webPropXray.valueArray.Add(translucent);
		OutMOI.properties.Add("Xray", webPropXray);

		const FWebMOIProperty* formPropVisible = WebProperties.Find(TEXT("Visible"));
		FWebMOIProperty webPropVisible = *formPropVisible;
		FString visible = HideRequests.Contains(FName(TEXT("WebPropertiesPanel"))) ? TEXT("false") : TEXT("true");
		webPropVisible.valueArray.Add(visible);
		OutMOI.properties.Add("Visible", webPropVisible);
		
		return true;
	}

	return false;
}