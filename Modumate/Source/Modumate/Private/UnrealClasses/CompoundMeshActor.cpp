// Copyright 2018 Modumate, Inc. All Rights Reserved.


#include "UnrealClasses/CompoundMeshActor.h"

#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Drafting/ModumateDraftingElements.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DrawingDesigner/DrawingDesignerLine.h"

#define DEBUG_NINE_SLICING 0

// Sets default values
ACompoundMeshActor::ACompoundMeshActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(USceneComponent::GetDefaultSceneRootVariableName());
}

// Called when the game starts or when spawned
void ACompoundMeshActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ACompoundMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACompoundMeshActor::MakeFromAssemblyPart(const FBIMAssemblySpec& ObAsm, int32 PartIndex, FVector Scale, bool bLateralInvert, bool bMakeCollision)
{
	// Figure out how many components we might need.

	int32 maxNumMeshes = ObAsm.Parts.Num();

	if (!ensureAlways(PartIndex >= 0 && PartIndex < maxNumMeshes))
	{
		return;
	}

	for (int32 compIdx = 0; compIdx < StaticMeshComps.Num(); ++compIdx)
	{
		if (UStaticMeshComponent* staticMeshComp = StaticMeshComps[compIdx])
		{
			if (compIdx < maxNumMeshes)
			{
				staticMeshComp->SetVisibility(false);
				staticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			else
			{
				staticMeshComp->DestroyComponent();
				StaticMeshComps[compIdx] = nullptr;
			}
			UseSlicedMesh[compIdx] = false;
		}
	}
	// Clear out StaticMeshComponents beyond what we need (SetNumZeroed leaves existing elements intact)
	StaticMeshComps.SetNumZeroed(maxNumMeshes);
	UseSlicedMesh.SetNumZeroed(maxNumMeshes);

	ResetProcMeshComponents(NineSliceComps, maxNumMeshes);
	ResetProcMeshComponents(NineSliceLowLODComps, maxNumMeshes);
	ClearCapGeometry();

	MaterialIndexMappings.SetNum(maxNumMeshes);

	USceneComponent* rootComp = GetRootComponent();

	const bool origDynamicStatus = GetIsDynamic();
	SetIsDynamic(true);

	FVector rootFlip = Scale.GetSignVector();
	Scale = Scale.GetAbs();

	// Update scaled part layout for assembly
	// TODO: determine when we can avoid re-calculating the part layout, ideally by comparing assembly by-value or by-key as well as the desired scale.
	if (!ensureAlways(CachedPartLayout.FromAssembly(ObAsm, Scale) == EBIMResult::Success))
	{
		return;
	}

	// Walk over part layout and build mesh components
	int32 numSlots = ObAsm.Parts.Num();

	if (!ensureAlways(numSlots == CachedPartLayout.PartSlotInstances.Num()))
	{
		return;
	}

	TArray<bool> partVisible;
	// Part 0 is the root of the whole assembly, so all parts will be active
	if (PartIndex > 0)
	{
		partVisible.SetNum(maxNumMeshes);

		for (int32 i = 0; i < maxNumMeshes; ++i)
		{
			int32 part = i;
			int32 sanity = 0;
			partVisible[i] = false;
			while (part != -1 && sanity++ < maxNumMeshes)
			{
				if (part == PartIndex)
				{
					partVisible[i] = true;
					break;
				}
				part = ObAsm.Parts[part].ParentSlotIndex;
			}
		}
	}

	for (int32 slotIdx = 0; slotIdx < numSlots; ++slotIdx)
	{
		if (partVisible.Num() > 0 && !partVisible[slotIdx])
		{
			continue;
		}

		const FBIMPartSlotSpec& assemblyPart = ObAsm.Parts[slotIdx];
		// Part[0] and potentially other parts are mesh-less containers used to store parenting values
		// All they need is their VariableValues set, there are no mesh components to make
		if (!assemblyPart.Mesh.EngineMesh.IsValid())
		{
			continue;
		}

		// Now make the mesh component and set it up using the cached transform data
		UStaticMesh* partMesh = assemblyPart.Mesh.EngineMesh.Get();

		// Make sure that there's a static mesh component for each part that has the engine mesh.
		UStaticMeshComponent* partStaticMeshComp = StaticMeshComps[slotIdx];
		if (partStaticMeshComp == nullptr)
		{
			partStaticMeshComp = NewObject<UStaticMeshComponent>(this);
			partStaticMeshComp->SetupAttachment(rootComp);
			AddOwnedComponent(partStaticMeshComp);
			partStaticMeshComp->RegisterComponent();
			StaticMeshComps[slotIdx] = partStaticMeshComp;
		}

		bool bMeshChanged = partStaticMeshComp->SetStaticMesh(partMesh);
		partStaticMeshComp->SetMobility(EComponentMobility::Movable);

		const FVector& partRelativePos = CachedPartLayout.PartSlotInstances[slotIdx].Location;
		FRotator partRotator = FRotator::MakeFromEuler(CachedPartLayout.PartSlotInstances[slotIdx].Rotation);
		FVector partNativeSize = assemblyPart.Mesh.NativeSize * UModumateDimensionStatics::InchesToCentimeters;

		FVector partScale = CachedPartLayout.PartSlotInstances[slotIdx].Size / partNativeSize;

		FVector partDesiredSize = CachedPartLayout.PartSlotInstances[slotIdx].Size;

#if DEBUG_NINE_SLICING
		DrawDebugCoordinateSystem(GetWorld(), GetActorTransform().TransformPosition(partRelativePos), GetActorRotation(), 8.0f, true, 1.f, 255, 0.5f);
#endif // DEBUG_NINE_SLICING

		FBox nineSliceInterior = assemblyPart.Mesh.NineSliceBox;
		nineSliceInterior.Min *= UModumateDimensionStatics::InchesToCentimeters;
		nineSliceInterior.Max *= UModumateDimensionStatics::InchesToCentimeters;
		FBox nativeExteriorSizes(nineSliceInterior.Min, partNativeSize - nineSliceInterior.Max);
		FVector minNativeExteriorSizes = nativeExteriorSizes.Min.ComponentMin(nativeExteriorSizes.Max);

		// Re-adjust 9 slicing parameters for parts that only stretch in one dimension
		// TODO: in this case, only create 3 slices, rather than 9.
		FVector desiredSizeDelta = partDesiredSize - partNativeSize;
		FVector originalInteriorSize = nineSliceInterior.GetSize();
		if ((originalInteriorSize.X == 0.0f) && (originalInteriorSize.Z != 0.0f))
		{
			originalInteriorSize.X = partNativeSize.X;
		}
		if ((originalInteriorSize.Z == 0.0f) && (originalInteriorSize.X != 0.0f))
		{
			originalInteriorSize.Z = partNativeSize.Z;
		}

		// If the interior size is less than minInteriorSizeFactor * the minimum of the exterior sizes,
		// then scale down all components in that dimension equally, so that the interior is always visible.
		FVector exteriorScale(1.0f);
		static float minInteriorSizeFactor = 1.0f;
		FVector desiredInteriorSize = partDesiredSize - (partNativeSize - originalInteriorSize);
		for (int32 axisIdx = 0; axisIdx < 3; ++axisIdx)
		{
			if (desiredInteriorSize.Component(axisIdx) < minInteriorSizeFactor * minNativeExteriorSizes.Component(axisIdx))
			{
				float totalUnscaledInteriorDim = minInteriorSizeFactor * minNativeExteriorSizes.Component(axisIdx) +
					nativeExteriorSizes.Min.Component(axisIdx) + nativeExteriorSizes.Max.Component(axisIdx);
				exteriorScale.Component(axisIdx) = partDesiredSize.Component(axisIdx) / totalUnscaledInteriorDim;
				desiredInteriorSize.Component(axisIdx) = exteriorScale.Component(axisIdx) * minInteriorSizeFactor * minNativeExteriorSizes.Component(axisIdx);
			}
		}

		int32 sliceCompIdxStart = 9 * slotIdx;
		FVector nineSliceInteriorExtent = nineSliceInterior.GetExtent();

		// Point and edge hosted objects do not support 9-slicing parts
		bool bPartIsStatic = true;
 		if (ObAsm.ObjectType != EObjectType::OTPointHosted &&
 			ObAsm.ObjectType != EObjectType::OTEdgeHosted)
		{
			bPartIsStatic =
				(nineSliceInterior.IsValid == 0) ||
				((nineSliceInteriorExtent.X == 0) && (nineSliceInteriorExtent.Z == 0));
		}

		// For static parts, we just need to reposition/scale this component
		if (bPartIsStatic)
		{
			partStaticMeshComp->SetVisibility(true);
			partStaticMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

			partStaticMeshComp->SetRelativeLocation(rootFlip * partRelativePos);
			partStaticMeshComp->SetRelativeRotation(partRotator);
			partStaticMeshComp->SetRelativeScale3D(rootFlip * partScale * CachedPartLayout.PartSlotInstances[slotIdx].FlipVector);

			UModumateFunctionLibrary::SetMeshMaterialsFromMapping(partStaticMeshComp, assemblyPart.ChannelMaterials);

			// And also, disable the proc mesh components that we won't be using for this slot
			for (int32 sliceIdx = 0; sliceIdx < 9; ++sliceIdx)
			{
				int32 compIdx = sliceCompIdxStart + sliceIdx;
				UProceduralMeshComponent* procMeshComp = NineSliceComps[compIdx];
				if (procMeshComp)
				{
					procMeshComp->DestroyComponent();
					NineSliceComps[compIdx] = nullptr;
				}
			}
		}
		// For 9-slice parts, we only use the StaticMeshComponent for reference,
		// and instead manipulate up to 9 ProceduralMeshComponents.
		else if (partMesh != nullptr)
		{
			partStaticMeshComp->SetVisibility(false);
			partStaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			TMap<FName, int32>& matIndexMapping = MaterialIndexMappings[slotIdx];
			bool bUpdateMaterials = false;

			int32 baseLodIndex = 0;
			int32 minLodIndex = partMesh->GetNumLODs() - 1;

			// generate meshes for both the 3D scene (NineSliceComps) and drafting (NineSliceLowLODComps)
			for (int32 meshesIdx = 0; meshesIdx <= 1; meshesIdx++)
			{
				int32 lodIndex = meshesIdx == 0 ? baseLodIndex : minLodIndex;
				TArray<UProceduralMeshComponent*>& comps = meshesIdx == 0 ? NineSliceComps : NineSliceLowLODComps;

				if (partMesh->HasValidRenderData(true, lodIndex))
				{
					bool bCreated = InitializeProcMeshComponent(comps, rootComp, sliceCompIdxStart);

					UProceduralMeshComponent* baseProcMeshComp = comps[sliceCompIdxStart];
					if (bMeshChanged && !bCreated)
					{
						baseProcMeshComp->ClearAllMeshSections();
					}

					if (bMeshChanged || bCreated)
					{
						UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(partStaticMeshComp, lodIndex, baseProcMeshComp, bMakeCollision);

						CalculateNineSliceComponents(comps, rootComp, lodIndex, sliceCompIdxStart, nineSliceInterior, partNativeSize, partStaticMeshComp);

						bUpdateMaterials = true;
					}
				}
			}

			if (bUpdateMaterials)
			{
				matIndexMapping.Empty();
				for (int32 matIdx = 0; matIdx < partMesh->GetStaticMaterials().Num(); ++matIdx)
				{
					const FStaticMaterial& meshMaterial = partMesh->GetStaticMaterials()[matIdx];
					matIndexMapping.Add(meshMaterial.MaterialSlotName, matIdx);
				}
			}

#if DEBUG_NINE_SLICING
			static FColor debugSliceColors[9] = {
				FColor::Red, FColor::Green, FColor::Blue,
				FColor::Yellow, FColor::Cyan, FColor::Magenta,
				FColor::Orange, FColor::Purple, FColor::Turquoise
			};
#endif // DEBUG_NINE_SLICING

			FBox partRelSliceBounds[9];
			const FBoxSphereBounds& meshBounds = partMesh->GetExtendedBounds();
			FVector meshMinExtension = (meshBounds.Origin - meshBounds.BoxExtent);
			FVector meshMaxExtension = (meshBounds.Origin + meshBounds.BoxExtent) - partNativeSize;

			for (int32 sliceIdx = 0; sliceIdx < 9; ++sliceIdx)
			{
				int32 compIdx = sliceCompIdxStart + sliceIdx;
				UProceduralMeshComponent* procMeshComp = NineSliceComps[compIdx];
				UProceduralMeshComponent* procMeshLowLODComp = NineSliceLowLODComps[compIdx];
				if (procMeshComp && (procMeshComp->GetNumSections() > 0))
				{
					FVector sliceRelativePos = partRelativePos;
					FVector sliceScale = FVector::OneVector;

					if (!partScale.Equals(FVector::OneVector))
					{
						FBox& newBounds = partRelSliceBounds[sliceIdx];
						newBounds.Min = meshMinExtension;
						newBounds.Max = partDesiredSize + meshMaxExtension;
						FBox originalBounds(meshBounds.Origin - meshBounds.BoxExtent, meshBounds.Origin + meshBounds.BoxExtent);

						switch (sliceIdx)
						{
						case 0:
							newBounds.Max = exteriorScale * nineSliceInterior.Min;
							originalBounds.Max = nineSliceInterior.Min;
							break;
						case 1:
							newBounds.Min.X = partRelSliceBounds[0].Max.X;
							newBounds.Max = partRelSliceBounds[0].Max;
							newBounds.Max.X += desiredInteriorSize.X;
							originalBounds.Min.X = nineSliceInterior.Min.X;
							originalBounds.Max.X = nineSliceInterior.Max.X;
							originalBounds.Max.Z = nineSliceInterior.Min.Z;
							break;
						case 2:
							newBounds.Min.X = partRelSliceBounds[1].Max.X;
							newBounds.Max.Z = partRelSliceBounds[0].Max.Z;
							originalBounds.Min.X = nineSliceInterior.Max.X;
							originalBounds.Max.Z = nineSliceInterior.Min.Z;
							break;
						case 3:
							newBounds.Min.Z = partRelSliceBounds[0].Max.Z;
							newBounds.Max = partRelSliceBounds[0].Max;
							newBounds.Max.Z += desiredInteriorSize.Z;
							originalBounds.Min.Z = nineSliceInterior.Min.Z;
							originalBounds.Max.X = nineSliceInterior.Min.X;
							originalBounds.Max.Z = nineSliceInterior.Max.Z;
							break;
						case 4:
							newBounds.Min = partRelSliceBounds[0].Max;
							newBounds.Max = partRelSliceBounds[3].Max;
							newBounds.Max.X += desiredInteriorSize.X;
							originalBounds.Min = nineSliceInterior.Min;
							originalBounds.Max = nineSliceInterior.Max;
							break;
						case 5:
							newBounds.Min = partRelSliceBounds[1].Max;
							newBounds.Max.Z = partRelSliceBounds[4].Max.Z;
							originalBounds.Min.X = nineSliceInterior.Max.X;
							originalBounds.Min.Z = nineSliceInterior.Min.Z;
							originalBounds.Max.Z = nineSliceInterior.Max.Z;
							break;
						case 6:
							newBounds.Min.Z = partRelSliceBounds[3].Max.Z;
							newBounds.Max.X = partRelSliceBounds[3].Max.X;
							originalBounds.Min.Z = nineSliceInterior.Max.Z;
							originalBounds.Max.X = nineSliceInterior.Min.X;
							break;
						case 7:
							newBounds.Min = partRelSliceBounds[3].Max;
							newBounds.Max.X = partRelSliceBounds[4].Max.X;
							originalBounds.Min.X = nineSliceInterior.Min.X;
							originalBounds.Min.Z = nineSliceInterior.Max.Z;
							originalBounds.Max.X = nineSliceInterior.Max.X;
							break;
						case 8:
							newBounds.Min = partRelSliceBounds[4].Max;
							originalBounds.Min = nineSliceInterior.Max;
							break;
						default:
							break;
						}

						newBounds.Min.Y = originalBounds.Min.Y = 0.0f;
						newBounds.Max.Y = partDesiredSize.Y;
						originalBounds.Max.Y = partNativeSize.Y;

						FVector originalBoundsSize = originalBounds.GetSize();
						FVector newBoundsSize = newBounds.GetSize();

						if ((originalBoundsSize.GetMin() <= 0.0f) || (newBoundsSize.GetMin() <= 0.0f))
						{
							procMeshComp->SetVisibility(false);
							procMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							continue;
						}

						sliceScale = newBoundsSize / originalBoundsSize;
						sliceRelativePos = CachedPartLayout.PartSlotInstances[slotIdx].Location + CachedPartLayout.PartSlotInstances[slotIdx].FlipVector * (newBounds.Min - (originalBounds.Min * sliceScale));
					}

					for (auto* comp : { procMeshComp, procMeshLowLODComp })
					{
						comp->SetRelativeLocation(rootFlip * sliceRelativePos);
						comp->SetRelativeRotation(FQuat::Identity);
						comp->SetRelativeRotation(partRotator);
						comp->SetRelativeScale3D(rootFlip * sliceScale * CachedPartLayout.PartSlotInstances[slotIdx].FlipVector);
						comp->SetVisibility(true);
						comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					}

					UseSlicedMesh[slotIdx] = true;

#if DEBUG_NINE_SLICING
					DrawDebugBox(GetWorld(), procMeshComp->Bounds.Origin, procMeshComp->Bounds.BoxExtent,
						FQuat::Identity, debugSliceColors[sliceIdx], true, 1.f, 255, 0.0f);
#endif // DEBUG_NINE_SLICING
				}
			}

			// TODO: reactivate when DDL 2 materials come online
			for (int32 sliceIdx = 0; sliceIdx < 9; ++sliceIdx)
			{
				int32 compIdx = sliceCompIdxStart + sliceIdx;
				UProceduralMeshComponent* procMeshComp = NineSliceComps[compIdx];
				if (procMeshComp)
				{
					UModumateFunctionLibrary::SetMeshMaterialsFromMapping(procMeshComp, assemblyPart.ChannelMaterials, &matIndexMapping);
				}

				procMeshComp = NineSliceLowLODComps[compIdx];
				if (procMeshComp)
				{
					UModumateFunctionLibrary::SetMeshMaterialsFromMapping(procMeshComp, assemblyPart.ChannelMaterials, &matIndexMapping);
					procMeshComp->SetVisibility(false);
				}
			}
		}
	}

	SetIsDynamic(origDynamicStatus); 
}

void ACompoundMeshActor::MakeFromAssembly(const FBIMAssemblySpec& ObAsm, FVector Scale, bool bLateralInvert, bool bMakeCollision)
{
	MakeFromAssemblyPart(ObAsm, 0, Scale, bLateralInvert, bMakeCollision);
}

void ACompoundMeshActor::SetupCapGeometry()
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!controller)
	{
		return;
	}

	ClearCapGeometry();
	int32 capMeshId = 0;

	// Create cap for procedural meshes
	const int32 numComponents = StaticMeshComps.Num();
	for (int32 component = 0; component < numComponents; ++component)
	{
		UStaticMeshComponent* staticMeshComponent = StaticMeshComps[component];
		if (staticMeshComponent == nullptr)
		{
			continue;
		}

		if (UseSlicedMesh[component])
		{   // Component has been nine-sliced.
			// All 9-slice procedural meshes are grouped according to their original static mesh
			TArray<UProceduralMeshComponent*> curMeshGroup;
			for (int32 slice = 9 * component; slice < 9 * (component + 1); ++slice)
			{
				curMeshGroup.Add(NineSliceLowLODComps[slice]);
			}
			if (curMeshGroup.Num() > 0)
			{
				for (int32 sectionId = 0; sectionId < curMeshGroup[0]->GetNumSections(); sectionId++)
				{
					UProceduralMeshComponent* curCapMesh = nullptr;
					if (GetOrAddProceduralMeshCap(capMeshId, curCapMesh))
					{
						if (UModumateGeometryStatics::CreateProcMeshCapFromPlane(curCapMesh,
							curMeshGroup, TArray<UStaticMeshComponent*>{},
							controller->GetCurrentCullingPlane(), sectionId, curMeshGroup[0]->GetMaterial(sectionId)))
						{
							capMeshId++;
						}
					}
				}
			}
		}
		else
		{
			UProceduralMeshComponent* curCapMesh = nullptr;
			if (GetOrAddProceduralMeshCap(capMeshId, curCapMesh))
			{
				if (UModumateGeometryStatics::CreateProcMeshCapFromPlane(curCapMesh,
					TArray<UProceduralMeshComponent*> {}, TArray<UStaticMeshComponent*>{staticMeshComponent},
					controller->GetCurrentCullingPlane(), 0, staticMeshComponent->GetMaterial(0)))
				{
					capMeshId++;
				}
			}
		}
	}
}

void ACompoundMeshActor::ClearCapGeometry()
{
	for (UProceduralMeshComponent* curCap : ProceduralMeshCaps)
	{
		curCap->ClearAllMeshSections();
	}
}

bool ACompoundMeshActor::ConvertProcMeshToLinesOnPlane(const FVector &PlanePosition, const FVector &PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges)
{
	for (auto procMesh : NineSliceComps)
	{
		UModumateGeometryStatics::ConvertProcMeshToLinesOnPlane(procMesh, PlanePosition, PlaneNormal, OutEdges);
	}
	return true;
}

bool ACompoundMeshActor::ConvertStaticMeshToLinesOnPlane(const FVector &PlanePosition, const FVector &PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges)
{
	for (auto staticMesh: StaticMeshComps)
	{
		UModumateGeometryStatics::ConvertStaticMeshToLinesOnPlane(staticMesh, PlanePosition, PlaneNormal, OutEdges);
	}
	return true;
}

void ACompoundMeshActor::UpdateLightFromLightConfig(UStaticMeshComponent* parentMesh, const FLightConfiguration &lightConfig, const FTransform &lightTransform)
{
	bool makePointLight = (lightConfig.LightIntensity > 0.f) && lightConfig.bAsSpotLight == false;
	bool makeSpotLight = (lightConfig.LightIntensity > 0.f) && lightConfig.bAsSpotLight;
	float lightIntensity = lightConfig.LightIntensity;
	FLinearColor lightColor = lightConfig.LightColor;

	// Point Light WIP
	if (makePointLight)
	{
		if (!PointLightComp)
		{
			PointLightComp = NewObject<UPointLightComponent>(this);
			PointLightComp->IntensityUnits = ELightUnits::Lumens;
			PointLightComp->SetMobility(EComponentMobility::Movable);
			PointLightComp->SetupAttachment(parentMesh);
			PointLightComp->RegisterComponent();
		}
		PointLightComp->SetRelativeTransform(lightTransform);
		PointLightComp->SetIntensity(lightIntensity);
		PointLightComp->SetLightColor(lightColor);
		//PointLight->SetIESTexture(lightProfile); // Should point light uses light profile?
	}
	else if (PointLightComp)
	{
		PointLightComp->DestroyComponent();
		PointLightComp = nullptr;
	}

	// Spot Light WIP
	if (makeSpotLight)
	{
		if (!SpotLightComp)
		{
			SpotLightComp = NewObject<USpotLightComponent>(this);
			SpotLightComp->IntensityUnits = ELightUnits::Lumens;
			SpotLightComp->SetMobility(EComponentMobility::Movable);
			SpotLightComp->SetupAttachment(parentMesh);
			SpotLightComp->RegisterComponent();
		}
		SpotLightComp->SetRelativeTransform(lightTransform);
		SpotLightComp->SetIntensity(lightIntensity);
		SpotLightComp->SetLightColor(lightColor);

		UTextureLightProfile* lightProfile = lightConfig.LightProfile.Get();
		if (lightProfile)
		{
			SpotLightComp->SetIESTexture(lightProfile);
		}
	}
	else if (SpotLightComp)
	{
		SpotLightComp->DestroyComponent();
		SpotLightComp = nullptr;
	}
}

void ACompoundMeshActor::RemoveAllLights()
{
	if (PointLightComp)
	{
		PointLightComp->DestroyComponent();
		PointLightComp = nullptr;
	}
	if (SpotLightComp)
	{
		SpotLightComp->DestroyComponent();
		SpotLightComp = nullptr;
	}
}

void ACompoundMeshActor::ResetProcMeshComponents(TArray<UProceduralMeshComponent*> &ProcMeshComps, int32 maxNumMeshes)
{
	for (int32 meshIdx = 0; (9 * meshIdx) < ProcMeshComps.Num(); ++meshIdx)
	{
		for (int32 sliceIdx = 0; sliceIdx < 9; ++sliceIdx)
		{
			int32 compIdx = 9 * meshIdx + sliceIdx;
			if (UProceduralMeshComponent *procMeshComp = ProcMeshComps[compIdx])
			{
				if (meshIdx < maxNumMeshes)
				{
					procMeshComp->SetVisibility(false);
					procMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
				else
				{
					procMeshComp->DestroyComponent();
					ProcMeshComps[compIdx] = nullptr;
				}
			}
		}
	}
	// Clear out ProceduralMeshComponents beyond what we need (SetNumZeroed leaves existing elements intact)
	ProcMeshComps.SetNumZeroed(9 * maxNumMeshes);
}

bool ACompoundMeshActor::InitializeProcMeshComponent(TArray<UProceduralMeshComponent*> &ProcMeshComps, USceneComponent *rootComp, int32 index)
{
	UProceduralMeshComponent *baseProcMeshComp = ProcMeshComps[index];
	if (baseProcMeshComp == nullptr)
	{
		baseProcMeshComp = NewObject<UProceduralMeshComponent>(this);
		baseProcMeshComp->SetMobility(EComponentMobility::Movable);
		baseProcMeshComp->SetupAttachment(rootComp);
		baseProcMeshComp->bUseComplexAsSimpleCollision = true;
		baseProcMeshComp->bGenerateMirroredCollision = true;
		baseProcMeshComp->bUseAsyncCooking = true;
		AddOwnedComponent(baseProcMeshComp);
		baseProcMeshComp->RegisterComponent();

		ProcMeshComps[index] = baseProcMeshComp;
		return true;
	}
	return false;
}

void ACompoundMeshActor::CalculateNineSliceComponents(TArray<UProceduralMeshComponent*> &ProcMeshComps, USceneComponent *rootComp, const int32 LodIndex, 
	int32 sliceCompIdxStart, FBox &nineSliceInterior, const FVector &partNativeSize, const UStaticMeshComponent* StaticMeshRef)
{
	UProceduralMeshComponent *baseProcMeshComp = ProcMeshComps[sliceCompIdxStart];

	baseProcMeshComp->SetWorldTransform(FTransform::Identity);

	// Clear out any potentially-existing nine-slice components that might be from another mesh.
	for (int32 sliceIdx = 1; sliceIdx < 9; ++sliceIdx)
	{
		int32 compIdx = sliceCompIdxStart + sliceIdx;
		UProceduralMeshComponent *procMeshComp = ProcMeshComps[compIdx];
		if (procMeshComp)
		{
			procMeshComp->DestroyComponent();
			ProcMeshComps[compIdx] = nullptr;
		}
	}

	FVector nineSliceInteriorExtent = nineSliceInterior.GetExtent();

	// If we're nine slicing in only one dimension, then adjust the interior box here.
	// TODO: don't perform all the slices if we're only using one dimension.
	if (nineSliceInteriorExtent.X == 0)
	{
		nineSliceInterior.Max.X = partNativeSize.X;
	}
	if (nineSliceInteriorExtent.Z == 0)
	{
		nineSliceInterior.Max.Z = partNativeSize.Z;
	}

	// Slice the ProceduralMeshComponent into 9 pieces such that they form this grid:
	// 6 7 8
	// 3 4 5
	// 0 1 2
	auto sliceProcMesh = [this, rootComp, &ProcMeshComps, sliceCompIdxStart, StaticMeshRef](int32 sliceIdxIn, int32 sliceIdxOut, const FVector &sliceOrigin, const FVector &sliceNormal)
	{
		UProceduralMeshComponent *sliceResult = nullptr;
		UKismetProceduralMeshLibrary::SliceProceduralMesh(ProcMeshComps[sliceCompIdxStart + sliceIdxIn],
			sliceOrigin, sliceNormal, true, sliceResult, EProcMeshSliceCapOption::NoCap, nullptr);
		sliceResult->AttachToComponent(rootComp, FAttachmentTransformRules::KeepWorldTransform);
		ProcMeshComps[sliceCompIdxStart + sliceIdxOut] = sliceResult;
	};

	static const FVector sliceDirLeft(-1.0f, 0.0f, 0.0f);
	static const FVector sliceDirDown(0.0f, 0.0f, -1.0f);

	sliceProcMesh(0, 1, nineSliceInterior.Min, sliceDirLeft);
	sliceProcMesh(1, 2, nineSliceInterior.Max, sliceDirLeft);

	sliceProcMesh(0, 3, nineSliceInterior.Min, sliceDirDown);
	sliceProcMesh(1, 4, nineSliceInterior.Min, sliceDirDown);
	sliceProcMesh(2, 5, nineSliceInterior.Min, sliceDirDown);

	sliceProcMesh(3, 6, nineSliceInterior.Max, sliceDirDown);
	sliceProcMesh(4, 7, nineSliceInterior.Max, sliceDirDown);
	sliceProcMesh(5, 8, nineSliceInterior.Max, sliceDirDown);
}

bool ACompoundMeshActor::GetOrAddProceduralMeshCap(int32 CapId, UProceduralMeshComponent*& OutMesh)
{
	if (CapId >= ProceduralMeshCaps.Num())
	{
		UProceduralMeshComponent* newMesh = NewObject<UProceduralMeshComponent>(this);
		newMesh->SetMobility(EComponentMobility::Movable);
		newMesh->SetupAttachment(GetRootComponent());
		newMesh->bUseAsyncCooking = true;
		newMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		newMesh->SetCastShadow(false);
		newMesh->SetWorldRotation(FRotator::ZeroRotator);
		newMesh->SetWorldScale3D(FVector::OneVector);
		AddOwnedComponent(newMesh);
		newMesh->RegisterComponent();
		ProceduralMeshCaps.Add(newMesh);
	}
	if (ensureMsgf(ProceduralMeshCaps.IsValidIndex(CapId),
		TEXT("ProceduralMeshCaps, Num: %d, has no valid index %d"), ProceduralMeshCaps.Num(), CapId))
	{
		OutMesh = ProceduralMeshCaps[CapId];
		OutMesh->SetWorldRotation(FRotator::ZeroRotator);
		OutMesh->SetWorldScale3D(FVector::OneVector);
	}
	return OutMesh != nullptr;
}

void ACompoundMeshActor::SetIsDynamic(bool dynamicStatus)
{
	if (dynamicStatus != bIsDynamic)
	{
		bIsDynamic = dynamicStatus;
		const EComponentMobility::Type newMobility = bIsDynamic ? EComponentMobility::Movable : EComponentMobility::Static;
		for (UStaticMeshComponent* mesh: StaticMeshComps)
		{
			if (mesh)
			{
				mesh->SetMobility(newMobility);
			}
		}
	}
}

float ACompoundMeshActor::GetPortalCenter(const UModumateDocument* Doc, const FGuid& AssemblyGUID) const
{
	float centerOffset = 0.0f;
	
	const UMeshComponent* panelMesh = nullptr;
	const UStaticMeshComponent* panelStaticMesh = nullptr;

	const FBIMAssemblySpec* assembly = Doc->GetPresetCollection().GetAssemblyByGUID(EToolMode::VE_DOOR, AssemblyGUID);
	assembly = assembly ? assembly : Doc->GetPresetCollection().GetAssemblyByGUID(EToolMode::VE_WINDOW, AssemblyGUID);
	if (assembly)
	{
		static const FString panelTagComponent(TEXT("Panel"));
		for (int32 slot = 0; slot < assembly->Parts.Num(); ++slot)
		{
			const auto& ncp = assembly->Parts[slot].NodeCategoryPath;
			if (ncp.Tags.Num() > 0 && ncp.Tags.Last() == panelTagComponent && StaticMeshComps[slot]
				&& StaticMeshComps[slot]->GetStaticMesh())
			{
				panelStaticMesh = StaticMeshComps[slot];
				panelMesh = UseSlicedMesh[slot] ? Cast<UMeshComponent>(NineSliceComps[9 * slot]) : StaticMeshComps[slot];
				break;
			}
		}
	}

	if (panelMesh)
	{
		FVector meshMin, meshMax;
		panelStaticMesh->GetLocalBounds(meshMin, meshMax);
		FTransform panelMeshTransform = panelMesh->GetRelativeTransform();
		centerOffset = panelMeshTransform.TransformPosition((meshMin + meshMax) / 2).Y;
	}

	return centerOffset;
}

bool ACompoundMeshActor::GetCutPlaneDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin) const
{
		TArray<TPair<FVector, FVector>> OutEdges;

		auto gameState = GetWorld()->GetGameState<AEditModelGameState>();
		auto moi = gameState->Document->ObjectFromActor(this);
		bool bIsCabinet = moi && moi->GetObjectType() == EObjectType::OTCabinet;
		FModumateLayerType layerType = bIsCabinet ? FModumateLayerType::kCabinetCutCarcass
			: FModumateLayerType::kOpeningSystemCutLine;

		const int32 numComponents = StaticMeshComps.Num();
		for (int32 component = 0; component < numComponents; ++component)
		{
			UStaticMeshComponent* staticMeshComponent = StaticMeshComps[component];
			if (staticMeshComponent == nullptr)
			{
				continue;
			}

			if (UseSlicedMesh[component])
			{   // Component has been nine-sliced.
				for (int32 slice = 9 * component; slice < 9 * (component + 1); ++slice)
				{
					UModumateGeometryStatics::ConvertProcMeshToLinesOnPlane(NineSliceLowLODComps[slice], Origin, Plane, OutEdges);
				}
			}
			else
			{
				UModumateGeometryStatics::ConvertStaticMeshToLinesOnPlane(staticMeshComponent, Origin, Plane, OutEdges);
			}

		}

		ModumateUnitParams::FThickness defaultThickness = ModumateUnitParams::FThickness::Points(0.3f);
		FMColor defaultColor = FMColor::Gray64;
		FMColor swingColor = FMColor::Gray160;
		static constexpr float defaultDoorOpeningDegrees = 90.0f;

		for (auto& edge: OutEdges)
		{
			FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Key);
			FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Value);

			TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
				FModumateUnitCoord2D::WorldCentimeters(start),
				FModumateUnitCoord2D::WorldCentimeters(end),
				defaultThickness, defaultColor);
			line->SetLayerTypeRecursive(layerType);
			ParentPage->Children.Add(line);
		}

		return OutEdges.Num() != 0;
}

void ACompoundMeshActor::GetFarDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane, const FBox2D& BoundingBox) const
{
	TArray <FDrawingDesignerLine> ddLines;
	GetDrawingDesignerLines(Plane, ddLines, 0.1f, 0.9205f, false);

	TArray<FEdge> clippedLines;
	for (const auto& ddLine: ddLines)
	{
		const FEdge edge(ddLine.P1, ddLine.P2);
		clippedLines.Append(ParentPage->lineClipping->ClipWorldLineToView(edge));
	}

	auto gameState = GetWorld()->GetGameState<AEditModelGameState>();
	auto moi = gameState->Document->ObjectFromActor(this);
	bool bIsCabinet = moi && moi->GetObjectType() == EObjectType::OTCabinet;
	FModumateLayerType layerType = bIsCabinet ? FModumateLayerType::kCabinetBeyond
		: FModumateLayerType::kOpeningSystemBeyond;

	FVector2D boxClipped0;
	FVector2D boxClipped1;
	for (const auto& clippedLine : clippedLines)
	{
		FVector2D vert0(clippedLine.Vertex[0]);
		FVector2D vert1(clippedLine.Vertex[1]);

		if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
		{

			TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
				FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
				FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
				ModumateUnitParams::FThickness::Points(0.125f), FMColor::Gray64);
			ParentPage->Children.Add(line);
			line->SetLayerTypeRecursive(layerType);
		}
	}
}


void ACompoundMeshActor::GetDrawingDesignerLines(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& Outlines, float MinLength,
	float AngleTolerance /*= 0.9205f*/, bool bFastMode /*= true*/) const
{
	TArray<FDrawingDesignerLined> lines;
	const double minLength2 = MinLength * MinLength;

	int32 linesDropped = 0;

	const int32 numComponents = StaticMeshComps.Num();
	for (int32 component = 0; component < numComponents; ++component)
	{
		const UStaticMeshComponent* staticMeshComponent = StaticMeshComps[component];
		if (staticMeshComponent == nullptr)
		{
			continue;
		}

		if (UseSlicedMesh[component])
		{   // Nine-sliced mesh:
			const int32 sliceStart = 9 * component;
			const int32 sliceEnd = 9 * (component + 1);
			for (int32 slice = sliceStart; slice < sliceEnd; ++slice)
			{
				UProceduralMeshComponent* mesh9Component = NineSliceLowLODComps[slice];

				if (mesh9Component == nullptr)
				{
					continue;
				}

				const FTransform3d componentToWorld(mesh9Component->GetComponentTransform());
				const int32 numSections = mesh9Component->GetNumSections();
				for (int32 section = 0; section < numSections; ++section)
				{
					const FProcMeshSection* meshSection = mesh9Component->GetProcMeshSection(section);
					if (meshSection == nullptr)
					{
						continue;
					}

					const TArray<FProcMeshVertex>& sectionVertices = meshSection->ProcVertexBuffer;
					const TArray<uint32>& sectionIndices = meshSection->ProcIndexBuffer;
					const int32 numVertices = sectionVertices.Num();
					const int32 numIndices = sectionIndices.Num();
					ensure(numIndices % 3 == 0);

					const int32 numTriangles = numIndices / 3;
					for (int32 t = 0; t < numTriangles; ++t)
					{
						FVector3d p0(componentToWorld.TransformPosition(sectionVertices[sectionIndices[3 * t    ]].Position));
						FVector3d p1(componentToWorld.TransformPosition(sectionVertices[sectionIndices[3 * t + 1]].Position));
						FVector3d p2(componentToWorld.TransformPosition(sectionVertices[sectionIndices[3 * t + 2]].Position));

						FVector3d N((p2 - p0).Cross(p1 - p0));
						double area2 = N.SquaredLength();
						N.Normalize();
						// Check for various degenerate cases.
						static constexpr double minSideLen = 0.005;
						if (area2 > 0.01)
						{
							if (p0.DistanceSquared(p1) >= minSideLen)
							{
								lines.Emplace(p0, p1, N);
							}
							if (p1.DistanceSquared(p2) >= minSideLen)
							{
								lines.Emplace(p1, p2, N);
							}
							if (p2.DistanceSquared(p0) >= minSideLen)
							{
								lines.Emplace(p2, p0, N);
							}
						}
					}
				}
			}
		}
		else
		{   // Static mesh:
			UStaticMesh* staticMesh = staticMeshComponent->GetStaticMesh();
			if (staticMesh == nullptr)
			{
				continue;
			}

			TArray<FVector2D> UVs;		// Unused
			TArray<FProcMeshTangent> tangents;	// Unused

			const FTransform3d componentToWorld(staticMeshComponent->GetComponentTransform());
			const int lodNumber = staticMesh->GetNumLODs() - 1;
			const int32 numSections = staticMesh->GetNumSections(lodNumber);
			for (int32 section = 0; section < numSections; ++section)
			{
				TArray<FVector> sectionVertices;
				TArray<int32> sectionIndices;
				TArray<FVector> sectionNormals;
				UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(staticMesh, lodNumber, section,
					sectionVertices, sectionIndices, sectionNormals, UVs, tangents);
				ensure(sectionIndices.Num() % 3 == 0);

				const int32 numTriangles = sectionIndices.Num() / 3;
				for (int32 t = 0; t < numTriangles; ++t)
				{
					FVector3d p0(componentToWorld.TransformPosition(sectionVertices[sectionIndices[3 * t]]));
					FVector3d p1(componentToWorld.TransformPosition(sectionVertices[sectionIndices[3 * t + 1]]));
					FVector3d p2(componentToWorld.TransformPosition(sectionVertices[sectionIndices[3 * t + 2]]));
					// Wind order is CW(?)
					FVector3d N(((p2 - p0).Cross(p1 - p0)).Normalized());
					if (p0.DistanceSquared(p1) >= minLength2)
					{
						lines.Emplace(p0, p1, N);
					}
					if (p1.DistanceSquared(p2) >= minLength2)
					{
						lines.Emplace(p1, p2, N);
					}
					if (p2.DistanceSquared(p0) >= minLength2)
					{
						lines.Emplace(p2, p0, N);
					}
				}
			}
		}
	}

	TArray<FDrawingDesignerLined> filteredLines;
	UModumateGeometryStatics::GetSilhouetteEdges(lines, ViewDirection, 0.01, AngleTolerance);
	for (const auto& line : lines)
	{
		if (line)
		{
			Outlines.Add(line);
		}
	}
}
