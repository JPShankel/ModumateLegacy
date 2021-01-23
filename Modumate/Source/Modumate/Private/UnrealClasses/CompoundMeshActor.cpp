// Copyright 2018 Modumate, Inc. All Rights Reserved.


#include "UnrealClasses/CompoundMeshActor.h"

#include "Engine/Engine.h"
#include "Algo/Unique.h"
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
#include "ModumateCore/ModumateStats.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Drafting/ModumateDraftingElements.h"

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Modumate Mesh To Lines"), STAT_ModumateMeshToLines, STATGROUP_Modumate);

using namespace Modumate;

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
		bool bPartIsStatic =
			(nineSliceInterior.IsValid == 0) ||
			((nineSliceInteriorExtent.X == 0) && (nineSliceInteriorExtent.Z == 0));

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

						CalculateNineSliceComponents(comps, rootComp, lodIndex, sliceCompIdxStart, nineSliceInterior, partNativeSize);

						bUpdateMaterials = true;
					}
				}
			}

			if (bUpdateMaterials)
			{
				matIndexMapping.Empty();
				for (int32 matIdx = 0; matIdx < partMesh->StaticMaterials.Num(); ++matIdx)
				{
					const FStaticMaterial& meshMaterial = partMesh->StaticMaterials[matIdx];
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
			const FBoxSphereBounds& meshBounds = partMesh->ExtendedBounds;
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
				UModumateFunctionLibrary::SetMeshMaterialsFromMapping(procMeshComp, assemblyPart.ChannelMaterials, &matIndexMapping);

				procMeshComp = NineSliceLowLODComps[compIdx];
				UModumateFunctionLibrary::SetMeshMaterialsFromMapping(procMeshComp, assemblyPart.ChannelMaterials, &matIndexMapping);
				procMeshComp->SetVisibility(false);
			}
				}
			}
	SetIsDynamic(origDynamicStatus); 
}

void ACompoundMeshActor::MakeFromAssembly(const FBIMAssemblySpec& ObAsm, FVector Scale, bool bLateralInvert, bool bMakeCollision)
{
	MakeFromAssemblyPart(ObAsm, 0, Scale, bLateralInvert, bMakeCollision);
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
	int32 sliceCompIdxStart, FBox &nineSliceInterior, const FVector &partNativeSize)
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
	auto sliceProcMesh = [this, rootComp, &ProcMeshComps, sliceCompIdxStart](int32 sliceIdxIn, int32 sliceIdxOut, const FVector &sliceOrigin, const FVector &sliceNormal)
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

bool ACompoundMeshActor::GetCutPlaneDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin) const
{
		TArray<TPair<FVector, FVector>> OutEdges;

		auto gameState = GetWorld()->GetGameState<AEditModelGameState>();
		auto moi = gameState->Document->ObjectFromActor(this);
		bool bIsCabinet = moi && moi->GetObjectType() == EObjectType::OTCabinet;
		Modumate::FModumateLayerType layerType = bIsCabinet ? Modumate::FModumateLayerType::kCabinetCutCarcass
			: Modumate::FModumateLayerType::kOpeningSystemCutLine;

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

		ModumateUnitParams::FThickness defaultThickness = ModumateUnitParams::FThickness::Points(0.1f);
		Modumate::FMColor defaultColor = Modumate::FMColor::Gray64;
		Modumate::FMColor swingColor = Modumate::FMColor::Gray160;
		static constexpr float defaultDoorOpeningDegrees = 90.0f;

		for (auto& edge: OutEdges)
		{
			FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Key);
			FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, edge.Value);

			TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
				FModumateUnitCoord2D::WorldCentimeters(start),
				FModumateUnitCoord2D::WorldCentimeters(end),
				defaultThickness, defaultColor);
			line->SetLayerTypeRecursive(layerType);
			ParentPage->Children.Add(line);
		}

		return OutEdges.Num() != 0;
}

void ACompoundMeshActor::GetFarDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane, const FBox2D& BoundingBox) const
{
	const FTransform& actorToWorld = ActorToWorld();
	const FVector viewNormal = Plane;

	TArray<FEdge> portalEdges;

	static auto lexicalEdgeCompare = [](const FEdge& a, const FEdge& b)
	{   // Compare lexicographically.
		for (int v = 0; v < 2; ++v)
		{
			if (a.Vertex[v].X < b.Vertex[v].X)
			{
				return true;
			}
			else if (a.Vertex[v].X > b.Vertex[v].X)
			{
				return false;
			}
			else if (a.Vertex[v].Y < b.Vertex[v].Y)
			{
				return true;
			}
			else if (a.Vertex[v].Y > b.Vertex[v].Y)
			{
				return false;
			}
		}
		return false;  // False if equal.
	};

	auto gameState = GetWorld()->GetGameState<AEditModelGameState>();
	auto moi = gameState->Document->ObjectFromActor(this);
	bool bIsCabinet = moi && moi->GetObjectType() == EObjectType::OTCabinet;
	Modumate::FModumateLayerType layerType = bIsCabinet ? Modumate::FModumateLayerType::kCabinetBeyond
		: Modumate::FModumateLayerType::kOpeningSystemBeyond;

	const int32 numComponents = StaticMeshComps.Num();

	for (int32 component = 0; component < numComponents; ++component)
	{
		const UStaticMeshComponent* staticMeshComponent = StaticMeshComps[component];
		if (staticMeshComponent == nullptr)
		{
			continue;
		}

		if (UseSlicedMesh[component])
		{   // Component has been nine-sliced.
			TArray<FVector> vertices;
			TArray<int32> indices;

			for (int32 slice = 9 * component; slice < 9 * (component + 1); ++slice)
			{
				UProceduralMeshComponent* meshComponent = NineSliceLowLODComps[slice];

				if (meshComponent == nullptr)
				{
					continue;
				}

				const FTransform localToWorld = meshComponent->GetRelativeTransform() * actorToWorld;
				int numSections = meshComponent->GetNumSections();
				for (int section = 0; section < numSections; ++section)
				{
					const FProcMeshSection* meshSection = meshComponent->GetProcMeshSection(section);
					if (meshSection == nullptr)
					{
						continue;
					}
					const auto& sectionVertices = meshSection->ProcVertexBuffer;
					const auto& sectionIndices = meshSection->ProcIndexBuffer;
					const int32 numIndices = sectionIndices.Num();
					const int32 numVertices = sectionVertices.Num();
					ensure(numIndices % 3 == 0);

					int32 indexOffset = vertices.Num();

					for (int32 v = 0; v < numVertices; ++v)
					{
						vertices.Add(localToWorld.TransformPosition(sectionVertices[v].Position));
					}
					for (int32 i = 0; i < numIndices; ++i)
					{
						indices.Add(sectionIndices[i] + indexOffset);
					}

				}

			}

			// Nine-slicing can introduce significant shifts in vertices and edges,
			// so use increased epsilon of 6 mm.
			DraftingLinesFromTriangles(vertices, indices, viewNormal, portalEdges, 0.6f);

		}
		else
		{
			const FTransform localToWorld = staticMeshComponent->GetRelativeTransform() * actorToWorld;
			UStaticMesh* staticMesh = staticMeshComponent->GetStaticMesh();
			if (staticMesh == nullptr)
			{
				continue;
			}

			FVector minPoint;
			FVector maxPoint;
			FBox boundingBox(ForceInit);
			staticMeshComponent->GetLocalBounds(minPoint, maxPoint);
			boundingBox += localToWorld.TransformPosition(minPoint);
			boundingBox += localToWorld.TransformPosition(maxPoint);

			if (!boundingBox.IsValid || ParentPage->lineClipping->IsBoxOccluded(boundingBox))
			{
				continue;
			}

			const int levelOfDetailIndex = staticMesh->GetNumLODs() - 1;
			const FStaticMeshLODResources& meshResources = staticMesh->GetLODForExport(levelOfDetailIndex);
			FVector viewNormalLocal = localToWorld.InverseTransformVector(viewNormal);

			TArray<FVector> positions;
			TArray<int32> indices;
			TArray<FVector> normals;
			TArray<FVector2D> UVs;
			TArray<FProcMeshTangent> tangents;

			int32 numSections = meshResources.Sections.Num();
			for (int32 section = 0; section < numSections; ++section)
			{
				TArray<FVector> sectionPositions;
				TArray<int32> sectionIndices;
				UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(staticMesh, levelOfDetailIndex, section,
					sectionPositions, sectionIndices, normals, UVs, tangents);
				ensure(sectionIndices.Num() % 3 == 0);

				const int32 numVerts = sectionPositions.Num();
				const int32 numIndices = sectionIndices.Num();

				int32 indexOffset = positions.Num();

				for (int32 v = 0; v < numVerts; ++v)
				{
					positions.Add(sectionPositions[v]);
				}
				for (int32 i = 0; i < numIndices; ++i)
				{
					indices.Add(sectionIndices[i] + indexOffset);
				}

			}

			TArray<FEdge> localEdges;
			// Portal static meshes tend to have a lot of fine detail, to use epsilon of 0.5 mm.
			DraftingLinesFromTriangles(positions, indices, viewNormalLocal, localEdges, 0.05f);
			for (const auto& edge: localEdges)
			{
				portalEdges.Add(FEdge(localToWorld.TransformPosition(edge.Vertex[0]), localToWorld.TransformPosition(edge.Vertex[1])) );
			}

		}
	}

	TArray<FEdge> clippedLines;
	for (const auto& edge : portalEdges)
	{
		clippedLines.Append(ParentPage->lineClipping->ClipWorldLineToView(edge));
	}

	// Eliminate identical lines in 2D...
	// Superfluous now that we eliminate over the whole draft, but more efficient.
	for (auto& edge : clippedLines)
	{   // Canonical form:
		if (edge.Vertex[0].X > edge.Vertex[1].X ||
			(edge.Vertex[0].X == edge.Vertex[1].X && edge.Vertex[0].Y > edge.Vertex[1].Y))
		{
			Swap(edge.Vertex[0], edge.Vertex[1]);
		}
	}
	Algo::Sort(clippedLines, lexicalEdgeCompare);

	int32 eraseIndex = Algo::Unique(clippedLines, [](const FEdge& a, const FEdge& b)
	{
		return a.Vertex[0].X == b.Vertex[0].X && a.Vertex[0].Y == b.Vertex[0].Y
			&& a.Vertex[1].X == b.Vertex[1].X && a.Vertex[1].Y == b.Vertex[1].Y;
	});
	clippedLines.RemoveAt(eraseIndex, clippedLines.Num() - eraseIndex);

	FVector2D boxClipped0;
	FVector2D boxClipped1;
	for (const auto& clippedLine : clippedLines)
	{
		FVector2D vert0(clippedLine.Vertex[0]);
		FVector2D vert1(clippedLine.Vertex[1]);

		if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
		{

			TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
				FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
				FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
				ModumateUnitParams::FThickness::Points(0.125f), Modumate::FMColor::Gray64);
			ParentPage->Children.Add(line);
			line->SetLayerTypeRecursive(layerType);
		}
	}
}

// Vertices separated by Epsilon are considered equivalent.
void ACompoundMeshActor::DraftingLinesFromTriangles(const TArray<FVector>& Vertices, const TArray<int32>& Indices,
	const FVector& ViewDirection, TArray<FEdge>& outEdges, float Epsilon /*= 0.4f*/) const
{
	SCOPE_MS_ACCUMULATOR(STAT_ModumateMeshToLines);

	const float EpsilonSquare = Epsilon * Epsilon;
	static constexpr float angleThreshold = 0.9205f;  // 23 degrees

	static auto lexicalVectorCompare = [](const FVector& a, const FVector& b)
	{
		if (a.X == b.X)
		{
			if (a.Y == b.Y)
			{
				return a.Z < b.Z;
			}
			return a.Y < b.Y;
		}
		return a.X < b.X;
	};

	struct FLocalEdge
	{
		FVector A;
		FVector B;
		FVector N;
		bool bIsValid { true };
		operator bool() const { return bIsValid; }
		void Normalize() { if (lexicalVectorCompare(B, A)) { Swap(A, B); } }
		bool operator==(const FLocalEdge& rhs) const
		{
			return A == rhs.A && B == rhs.B || A == rhs.B && B == rhs.A;
		}
		bool operator<(const FLocalEdge& rhs) const
		{
			return A == rhs.A ? lexicalVectorCompare(B, rhs.B) : lexicalVectorCompare(A, rhs.A);
		}
	};

	TArray<FLocalEdge> edges;

	const int32 numTriangles = Indices.Num() / 3;
	for (int32 triangle = 0; triangle < numTriangles; ++triangle)
	{

		FVector vert0 = Vertices[Indices[triangle * 3]];
		FVector vert1 = Vertices[Indices[triangle * 3 + 1]];
		FVector vert2 = Vertices[Indices[triangle * 3 + 2]];
		if (FVector::DistSquared(vert0, vert1) > EpsilonSquare && FVector::DistSquared(vert0, vert2) > EpsilonSquare
			&& FVector::DistSquared(vert1, vert2) > EpsilonSquare)
		{
			FVector triNormal = ((vert2 - vert0) ^ (vert1 - vert0)).GetSafeNormal();
			if ((ViewDirection | triNormal) <= 0.0f)
			{
				edges.Add({ vert0, vert1, triNormal });
				edges.Add({ vert1, vert2, triNormal });
				edges.Add({ vert2, vert0, triNormal });
			}
		}
	}

	const int numEdges = edges.Num();
	for (auto& edge : edges)
	{
		edge.Normalize();
	}

	// Brute-force search for internal lines.
	for (int32 e1 = 0; e1 < numEdges; ++e1)
	{
		for (int32 e2 = e1 + 1; e2 < numEdges; ++e2)
		{
			auto& edge1 = edges[e1];
			auto& edge2 = edges[e2];
			if (FVector::DistSquared(edge1.A, edge2.A) < EpsilonSquare && FVector::DistSquared(edge1.B, edge2.B) < EpsilonSquare
				&& FMath::Abs(edge1.N | edge2.N) > angleThreshold)
			{
				edge1.bIsValid = false;
				edge2.bIsValid = false;
			}
		}
	}

	for (auto& edge: edges)
	{
		if (edge)
		{
			outEdges.Emplace(edge.A, edge.B);
		}
	}

}
