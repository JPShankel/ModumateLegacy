// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/CompoundMeshActor.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "Engine/Engine.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateUnits.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"

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

void ACompoundMeshActor::MakeFromAssembly(const FBIMAssemblySpec &obAsm, const FVector &scale, bool bLateralInvert, bool bMakeCollision)
{
	// Figure out how many components we might need.

	int32 maxNumMeshes = obAsm.Parts.Num();

	for (int32 compIdx = 0; compIdx < StaticMeshComps.Num(); ++compIdx)
	{
		if (UStaticMeshComponent *staticMeshComp = StaticMeshComps[compIdx])
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
		}
	}
	// Clear out StaticMeshComponents beyond what we need (SetNumZeroed leaves existing elements intact)
	StaticMeshComps.SetNumZeroed(maxNumMeshes);

	ResetProcMeshComponents(NineSliceComps, maxNumMeshes);
	ResetProcMeshComponents(NineSliceLowLODComps, maxNumMeshes);

	MaterialIndexMappings.SetNum(maxNumMeshes);

	USceneComponent* rootComp = GetRootComponent();

	const bool origDynamicStatus = GetIsDynamic();
	SetIsDynamic(true);

	TMap<FString, float> vars;
	int32 numSlots = obAsm.Parts.Num();

	// TODO: we assume the first part is the global "parent," to be refactored with parenting info in DDL tables
	if (numSlots > 0)
	{
		const FVector& nativeSize = obAsm.Parts[0].Mesh.NativeSize;
		vars.Add(TEXT("Parent.NativeSizeX"), nativeSize.X);
		vars.Add(TEXT("Parent.NativeSizeY"), nativeSize.Y);
		vars.Add(TEXT("Parent.NativeSizeZ"), nativeSize.Z);
	}

	for (int32 slotIdx = 0; slotIdx < numSlots; ++slotIdx)
	{
		const FBIMPartSlotSpec& assemblyPart = obAsm.Parts[slotIdx];

		if (!assemblyPart.Mesh.EngineMesh.IsValid())
		{
			continue;
		}

		const FVector& nativeSize = obAsm.Parts[slotIdx].Mesh.NativeSize;
		vars.Add(TEXT("Part.NativeSizeX"), nativeSize.X);
		vars.Add(TEXT("Part.NativeSizeY"), nativeSize.Y);
		vars.Add(TEXT("Part.NativeSizeZ"), nativeSize.Z);

		UStaticMesh *partMesh = assemblyPart.Mesh.EngineMesh.Get();

		// Make sure that there's a static mesh component for each part that has the engine mesh.
		UStaticMeshComponent *partStaticMeshComp = StaticMeshComps[slotIdx];
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

		FVector partLocation,partEulers; 
		assemblyPart.Translation.Evaluate(vars,partLocation);
		assemblyPart.Orientation.Evaluate(vars, partEulers);
		FRotator partRotator = FRotator::MakeFromEuler(partEulers);

		FVector partNativeSize = assemblyPart.Mesh.NativeSize;
		FVector partDesiredSize = partNativeSize;
		FVector partScale = FVector::OneVector;


#if DEBUG_NINE_SLICING
			DrawDebugCoordinateSystem(GetWorld(), GetActorTransform().TransformPosition(partLocation), GetActorRotation(), 8.0f, false, -1.f, 255, 0.5f);
#endif // DEBUG_NINE_SLICING

		// TODO: add flipping and transverse operations
		FVector partFlip = FVector::OneVector;
		FBox nineSliceInterior = assemblyPart.Mesh.NineSliceBox;
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

			partStaticMeshComp->SetRelativeLocation(partLocation);
			partStaticMeshComp->SetRelativeRotation(partRotator);
			partStaticMeshComp->SetRelativeScale3D(partScale * partFlip);

#if 0
			UModumateFunctionLibrary::SetMeshMaterialsFromMapping(partStaticMeshComp, portalConfig.MaterialsPerChannel);
#endif

			// And also, disable the proc mesh components that we won't be using for this slot
			for (int32 sliceIdx = 0; sliceIdx < 9; ++sliceIdx)
			{
				int32 compIdx = sliceCompIdxStart + sliceIdx;
				UProceduralMeshComponent *procMeshComp = NineSliceComps[compIdx];
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

			TMap<FName, int32> &matIndexMapping = MaterialIndexMappings[slotIdx];
			bool bUpdateMaterials = false;

			int32 baseLodIndex = 0;
			int32 minLodIndex = partMesh->GetNumLODs() - 1;

			// generate meshes for both the 3D scene (NineSliceComps) and drafting (NineSliceLowLODComps)
			for (int32 meshesIdx = 0; meshesIdx <= 1; meshesIdx++)
			{
				int32 lodIndex = meshesIdx == 0 ? baseLodIndex : minLodIndex;
				TArray<UProceduralMeshComponent*> &comps = meshesIdx == 0 ? NineSliceComps : NineSliceLowLODComps;

				if (partMesh->HasValidRenderData(true, lodIndex))
				{
					bool bCreated = InitializeProcMeshComponent(comps, rootComp, sliceCompIdxStart);

					UProceduralMeshComponent *baseProcMeshComp = comps[sliceCompIdxStart];
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
					const FStaticMaterial &meshMaterial = partMesh->StaticMaterials[matIdx];
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
			const FBoxSphereBounds &meshBounds = partMesh->ExtendedBounds;
			FVector meshMinExtension = (meshBounds.Origin - meshBounds.BoxExtent);
			FVector meshMaxExtension = (meshBounds.Origin + meshBounds.BoxExtent) - partNativeSize;

			for (int32 sliceIdx = 0; sliceIdx < 9; ++sliceIdx)
			{
				int32 compIdx = sliceCompIdxStart + sliceIdx;
				UProceduralMeshComponent *procMeshComp = NineSliceComps[compIdx];
				UProceduralMeshComponent *procMeshLowLODComp = NineSliceLowLODComps[compIdx];
				if (procMeshComp && (procMeshComp->GetNumSections() > 0))
				{
					FVector relativePos = partLocation;
					FVector sliceScale = FVector::OneVector;

					if (!partScale.Equals(FVector::OneVector))
					{
						FBox &newBounds = partRelSliceBounds[sliceIdx];
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
						relativePos = partLocation + partFlip * (newBounds.Min - (originalBounds.Min * sliceScale));
					}

					for (auto* comp : { procMeshComp, procMeshLowLODComp })
					{
						comp->SetRelativeLocation(relativePos);
						comp->SetRelativeRotation(FQuat::Identity);
						comp->SetRelativeRotation(partRotator);
						comp->SetRelativeScale3D(sliceScale * partFlip);
						comp->SetVisibility(true);
						comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					}

#if DEBUG_NINE_SLICING
					DrawDebugBox(GetWorld(), procMeshComp->Bounds.Origin, procMeshComp->Bounds.BoxExtent,
						FQuat::Identity, debugSliceColors[sliceIdx], false, -1.f, 255, 0.0f);
#endif // DEBUG_NINE_SLICING
				}
			}

			// TODO: reactivate when DDL 2 materials come online
#if 0
			for (int32 sliceIdx = 0; sliceIdx < 9; ++sliceIdx)
			{
				int32 compIdx = sliceCompIdxStart + sliceIdx;
				UProceduralMeshComponent *procMeshComp = NineSliceComps[compIdx];
				UModumateFunctionLibrary::SetMeshMaterialsFromMapping(procMeshComp, portalConfig.MaterialsPerChannel, &matIndexMapping);

				procMeshComp = NineSliceLowLODComps[compIdx];
				UModumateFunctionLibrary::SetMeshMaterialsFromMapping(procMeshComp, portalConfig.MaterialsPerChannel, &matIndexMapping);
				procMeshComp->SetVisibility(false);
			}
#endif

		}
	}
	SetIsDynamic(origDynamicStatus);
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
				if (meshIdx >= maxNumMeshes)
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
