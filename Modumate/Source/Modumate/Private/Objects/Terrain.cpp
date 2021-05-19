// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/Terrain.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2DVertex.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/DynamicTerrainActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"


AMOITerrain::AMOITerrain()
{

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
	{
		UpdateTerrainActors();
		break;
	}

	case EObjectDirtyFlags::Visuals:
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

	return true;
}

void AMOITerrain::PreDestroy()
{
	for (const auto& actor: TerrainActors)
	{
		actor->Destroy();
	}

	TerrainActors.Empty();

	Super::PreDestroy();
}

void AMOITerrain::UpdateTerrainActors()
{
	const auto graph2d = GetDocument()->FindSurfaceGraph(ID);
	int32 numTerrainPatches = 0;

	if (ensure(graph2d))
	{
		const auto& polygons = graph2d->GetPolygons();
		for (const auto& polygonKvp: polygons)
		{
			const auto& polygon = polygonKvp.Value;
			if (polygon.bInterior || polygon.ContainingPolyID != MOD_ID_NONE)
			{
				continue;
			}

			ADynamicTerrainActor* actor;
			if (TerrainActors.Num() <= numTerrainPatches)
			{
				actor = GetWorld()->SpawnActor<ADynamicTerrainActor>(ADynamicTerrainActor::StaticClass(), FTransform(FQuat::Identity, GetLocation() ));
				ensure(SetupTerrainMaterial(actor));
				TerrainActors.Add(actor);
			}
			else
			{
				actor = TerrainActors[numTerrainPatches];
			}

			if (!ensure(actor))
			{
				return;
			}

			TArray<FVector2D> perimeterPoints2D;
			TArray<FVector> heightPoints;
			TSet<int32> vertexIDs;

			TFunction<void (int32)> addVertexHeights = [&heightPoints, &vertexIDs, graph2d, this, &addVertexHeights](int32 Polygon)
			{
				for (int32 v: graph2d->FindPolygon(Polygon)->VertexIDs)
				{
					if (!vertexIDs.Contains(v))
					{
						FVector2D graph2dPosition = graph2d->GetVertices()[v].Position;
						heightPoints.Add(GraphToWorldPosition(graph2dPosition, InstanceData.Heights[v]));
						vertexIDs.Add(v);
					}
				}

				// Add all contained polys:
				for (int32 subPoly : graph2d->FindPolygon(Polygon)->ContainedPolyIDs)
				{
					addVertexHeights(subPoly);
				}

			};

			for (int32 v: polygon.VertexIDs)
			{
				ensure(InstanceData.Heights.Find(v));
				float height = InstanceData.Heights[v];

				FVector2D graph2dPosition = graph2d->GetVertices()[v].Position;
				FVector planePosition = InstanceData.Origin + graph2dPosition.X * FVector::ForwardVector + graph2dPosition.Y * FVector::RightVector;
				FVector position = planePosition + height * FVector::UpVector;

				perimeterPoints2D.Add(graph2dPosition);
			}

			TSet<int32> interiorPolys;
			for (int32 edge: polygon.Edges)
			{
				interiorPolys.Add(graph2d->FindEdge(edge)->LeftPolyID);
				interiorPolys.Add(graph2d->FindEdge(edge)->RightPolyID);
			}
			for (int32 poly: interiorPolys)
			{
				addVertexHeights(poly);
			}

			if (perimeterPoints2D.Num() >= 3)
			{
				actor->TestSetupTerrainGeometryGTE(perimeterPoints2D, heightPoints, TArray<FPolyHole2D>(), true, false);
			}

			++numTerrainPatches;
		}

		for(int32 deleteActor = numTerrainPatches; deleteActor < TerrainActors.Num(); ++deleteActor)
		{
			TerrainActors[deleteActor]->Destroy();
		}
		TerrainActors.SetNum(numTerrainPatches);
	}
}

bool AMOITerrain::SetupTerrainMaterial(ADynamicTerrainActor* Actor)
{
	if (!ensure(Actor))
	{
		return false;
	}

	auto* gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	UMaterialInstanceDynamic* dynamicMat = Cast<UMaterialInstanceDynamic>(Actor->Mesh->GetMaterial(0));
	if (dynamicMat == nullptr)
	{
		dynamicMat = UMaterialInstanceDynamic::Create(gameMode->TerrainMaterial, Actor->Mesh);
		Actor->TerrainMaterial = dynamicMat;

		Actor->GrassStaticMesh = gameMode->GrassMesh;
		UMaterialInstance* grassMat = Cast<UMaterialInstance>(Actor->GrassMesh->GetMaterial(0));
		if (grassMat == nullptr)
		{
			Actor->GrassMesh->SetMaterial(0, gameMode->GrassMaterial);
		}
	}

	return true;
}

FVector AMOITerrain::GraphToWorldPosition(FVector2D GraphPos, double Height /*= 0.0*/) const
{
	return GraphPos.X * FVector::ForwardVector + GraphPos.Y * FVector::RightVector
		+ Height * FVector::UpVector;
}
