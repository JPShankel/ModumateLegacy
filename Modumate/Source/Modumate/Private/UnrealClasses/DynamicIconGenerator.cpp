// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UnrealClasses/DynamicIconGenerator.h"

#include "Kismet/KismetRenderingLibrary.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "ModumateCore/ModumateUnits.h"
#include "Objects/Cabinet.h"
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "Runtime/Engine/Classes/Components/RectLightComponent.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ThumbnailCacheManager.h"
#include "Objects/LayeredObjectInterface.h"

using namespace Modumate;

// Sets default values
ADynamicIconGenerator::ADynamicIconGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Setup spring arm and capture
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);

	SceneCaptureComp = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComp"));
	SceneCaptureComp->SetupAttachment(SpringArm);

	// Create lights
	RectLight = CreateDefaultSubobject<URectLightComponent>(TEXT("RectLight"));
	RectLight->SetupAttachment(Root);
	RectLight1 = CreateDefaultSubobject<URectLightComponent>(TEXT("RectLight1"));
	RectLight1->SetupAttachment(Root);

	// Static Mesh
	IconStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IconStaticMesh"));
	IconStaticMesh->SetupAttachment(Root);
	IconStaticMesh->SetMobility(EComponentMobility::Movable);
	IconStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IconStaticMesh->CastShadow = false;
	IconStaticMesh->SetVisibility(false);

	// IconSphereMesh
	IconSphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IconSphereMesh"));
	IconSphereMesh->SetupAttachment(Root);
	IconSphereMesh->SetMobility(EComponentMobility::Movable);
	IconSphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IconSphereMesh->CastShadow = false;
	IconSphereMesh->SetVisibility(false);

	// IconCubeMesh
	IconCubeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IconCubeMesh"));
	IconCubeMesh->SetupAttachment(Root);
	IconCubeMesh->SetMobility(EComponentMobility::Movable);
	IconCubeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IconCubeMesh->CastShadow = false;
	IconCubeMesh->SetVisibility(false);
}

// Called when the game starts or when spawned
void ADynamicIconGenerator::BeginPlay()
{
	Super::BeginPlay();

	Gamemode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	GameState = Cast<AEditModelGameState>(GetWorld()->GetGameState());
	Controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());

	FActorSpawnParameters dynamicMeshSpawnParams;
	dynamicMeshSpawnParams.Name = FName(TEXT("IconDynamicMeshActor"));
	IconDynamicMeshActor = GetWorld()->SpawnActor<ADynamicMeshActor>(Gamemode->DynamicMeshActorClass.Get(), dynamicMeshSpawnParams);
	IconDynamicMeshActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);

	FActorSpawnParameters compoundMeshSpawnParams;
	compoundMeshSpawnParams.Name = FName(TEXT("IconCompoundMeshActor"));
	IconCompoundMeshActor = GetWorld()->SpawnActor<ACompoundMeshActor>(compoundMeshSpawnParams);
	IconCompoundMeshActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);

	DynCustomMaterial = IconSphereMesh->CreateDynamicMaterialInstance(0, CustomMaterialBase);

	IconRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), RenderTargetSize, RenderTargetSize, ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor::Black, true);
}

void ADynamicIconGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ADynamicIconGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool ADynamicIconGenerator::SetIconMeshForAssembly(const FBIMPresetCollectionProxy& PresetCollection, const FGuid& AsmKey, bool bAllowOverwrite)
{
	const FBIMPresetInstance* childPreset = PresetCollection.PresetFromGUID(AsmKey);
	if (childPreset == nullptr)
	{
		return false;
	}

	const FBIMAssemblySpec* assembly = PresetCollection.AssemblySpecFromGUID(childPreset->ObjectType,AsmKey);
	if (!assembly)
	{
		return false;
	}

	bool captureSuccess = SetIconMeshForAssemblyType(*assembly, IconRenderTarget, BIM_ROOT_PART, true);
	if (captureSuccess)
	{
		UTexture2D* outSavedTexture = nullptr;
		return UThumbnailCacheManager::SaveThumbnailFromPresetKey(IconRenderTarget, AsmKey, outSavedTexture, this, bAllowOverwrite);
	}

	return false;
}

bool ADynamicIconGenerator::SetIconMeshForAssembly(const FBIMPresetCollectionProxy& PresetCollection, const FGuid& AsmKey, UMaterialInterface*& OutMaterial, bool bAllowOverwrite)
{
	OutMaterial = nullptr;
	UTexture2D* iconTexture = nullptr;

	if (!bAllowOverwrite && GetSavedIconFromPreset(AsmKey, iconTexture))
	{
		OutMaterial = CreateMaterialForIconTexture(iconTexture);
		return true;
	}

	if (SetIconMeshForAssembly(PresetCollection, AsmKey, bAllowOverwrite) && GetSavedIconFromPreset(AsmKey, iconTexture))
	{
		OutMaterial = CreateMaterialForIconTexture(iconTexture);
		return true;
	}
	
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForBIMDesigner(const FBIMPresetCollectionProxy& PresetCollection, bool bUseDependentPreset, const FGuid& PresetID, UMaterialInterface*& OutMaterial, const FBIMEditorNodeIDType& NodeID, bool bCanCache)
{
	OutMaterial = nullptr;

	const FBIMPresetInstance* preset = PresetCollection.PresetFromGUID(PresetID);
	if (preset == nullptr)
	{
		return false;
	}

	// First go through the presets that don't need render capture for icon
	switch (preset->NodeScope)
	{
	case EBIMValueScope::Dimension: // Dimension icon is a static material, skip render capture
		OutMaterial = IconDimensionMaterial;
		return true;
	case EBIMValueScope::Pattern:
		return SetIconFromTextureAsset(PresetID, OutMaterial);
	}

	// Attempt to use cached icon first, make new if not available
	UTexture2D* outTexture = nullptr;
	if (bCanCache && GetSavedIconFromPreset(PresetID, outTexture))
	{
		UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconMaterial, this);
		dynMat->SetTextureParameterValue(MaterialIconTextureParamName, outTexture);
		OutMaterial = dynMat;
		return true;
	}

	// The following presets need render capture for icon
	bool bCaptureSuccess = false;
	// Some object types have special rule to render root node, ex: cabinet may not need extruded mesh on non root node
	bool fromRootNode = false;

	// TODO: Ideally this function will be in FBIMPresetEditor, in InstancePool of BIMDesigner
	// If this assembly is a part of its parent assembly, find the partID associated with the meshes needed to isolate for render
	if (preset->NodeScope == EBIMValueScope::Assembly || preset->NodeScope == EBIMValueScope::Part)
	{
		FString slotID;
		const FBIMPresetEditorNodeSharedPtr nodeInst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
		if (nodeInst && nodeInst->ParentInstance.Pin())
		{
			TArray<FBIMPresetPartSlot> partSlots;
			nodeInst->ParentInstance.Pin()->GetPartSlots(partSlots);
			for (int32 i = 0; i < partSlots.Num(); ++i)
			{
				if (partSlots[i].SlotPresetGUID == nodeInst->MyParentPartSlot)
				{
					const FBIMPresetInstance* slotPreset = PresetCollection.PresetFromGUID(partSlots[i].SlotPresetGUID);
					slotPreset->Properties.TryGetProperty(EBIMValueScope::Slot, BIMPropertyNames::ID, slotID);
					break;
				}
			}
		}

		if (nodeInst && !nodeInst->ParentInstance.Pin())
		{
			fromRootNode = true;
		}

		// Match slotID to the part inside the CraftingAssembly
		int32 assemblyPartIndex = 0; // 0 = defaulted to full assembly
		if (!slotID.IsEmpty())
		{
			for (int32 i = 0; i < Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly.Parts.Num(); ++i)
			{
				if (Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly.Parts[i].SlotID == slotID)
				{
					assemblyPartIndex = i;
					break;
				}
			}
		}

		// Stairs do not have slotIDs, find riser vs tread instead
		if (Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly.ObjectType == EObjectType::OTStaircase)
		{
			EBIMPinTarget target;
			if (ensureAlways(Controller->EditModelUserWidget->BIMDesigner->InstancePool.GetBIMPinTargetForChildNode(NodeID, target) == EBIMResult::Success))
			{
				if (target == EBIMPinTarget::Tread)
				{
					assemblyPartIndex = StairLayerTreadAssemblyPartIndex;
				}
				else if (target == EBIMPinTarget::Riser)
				{
					assemblyPartIndex = StairLayerRiserAssemblyPartIndex;
				}
			}
		}

		// Check if this icon is used for slot based swapping menu
		if (bUseDependentPreset && assemblyPartIndex > 0)
		{
			/*
			* To render a swapped part on a given assembly, we:
			* 1) Find the part's parent preset
			* 2) Swap in the candidate
			* 3) Update the cached preset collection
			* 4) Make an assembly
			* 5) Render an icon for it
			* 6) Restore the cached preset collection
			*/
			auto partNode = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
			if (partNode.IsValid() && partNode.Get()->ParentInstance.IsValid())
			{
				const FBIMAssemblySpec& craftingSpec = Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly;
				if (!ensureAlways(assemblyPartIndex < craftingSpec.Parts.Num()))
				{
					return false;
				}

				FGuid slotGUID = craftingSpec.Parts[assemblyPartIndex].SlotGUID;

				auto parentNode = partNode.Get()->ParentInstance.Pin();

				// A swapped part better have a parent
				if (!ensureAlways(parentNode))
				{
					return false;
				}

				// Make a copy of the parent node's preset and swap in the candidate part
				FBIMPresetInstance parentPreset = parentNode->Preset;
				bCaptureSuccess = false;
				for (auto& slot : parentPreset.PartSlots)
				{
					if (slot.SlotPresetGUID == slotGUID)
					{
						// No need for a delta here as we're not updating the document
						slot.PartPresetGUID = PresetID;

						FBIMPresetCollectionProxy alteredPartCollection(PresetCollection);
						alteredPartCollection.OverridePreset(parentPreset);

						// Make an assembly with the altered preset
						FBIMAssemblySpec assemblySpec;
						// Turn off verbose error reporting, needed for data validation but annoying for icons that are allowed to fail
						bool bEnsureOnFormulaError = FBIMPartLayout::bEnsureOnFormulaError;
						FBIMPartLayout::bEnsureOnFormulaError = false;
						if (assemblySpec.FromPreset(*Gamemode->ObjectDatabase, alteredPartCollection, craftingSpec.PresetGUID) == EBIMResult::Success)
						{
							FBIMPartLayout::bEnsureOnFormulaError = bEnsureOnFormulaError;
							bCaptureSuccess = SetIconMeshForAssemblyType(assemblySpec, IconRenderTarget, assemblyPartIndex, fromRootNode);
						}
						else
						{
							FBIMPartLayout::bEnsureOnFormulaError = bEnsureOnFormulaError;
							OutMaterial = IconSwapWarningMaterial;

							return true;
						}

						break;
					}
				}
				if (!bCaptureSuccess)
				{
					return false;
				}
			}
		}
		else
		{
			// If this preset is rendering as assembly, make sure it's the same object type
			// ex: Prevent wall preset form being render as stair tread assembly 
			if (preset->ObjectType != EObjectType::OTNone && 
				preset->ObjectType != Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly.ObjectType)
			{
				OutMaterial = IconSwapWarningMaterial;
				return true;
			}
			bCaptureSuccess = SetIconMeshForAssemblyType(Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly, IconRenderTarget, assemblyPartIndex, fromRootNode);
		}
	}

	switch (preset->NodeScope)
	{
	case EBIMValueScope::RawMaterial:
		bCaptureSuccess = SetIconMeshForRawMaterial(PresetID, IconRenderTarget);
		break;
	case EBIMValueScope::Profile:
		bCaptureSuccess = SetIconMeshForProfile(PresetID, IconRenderTarget);
		break;
	case EBIMValueScope::Material:
		bCaptureSuccess = SetIconMeshForMaterial(PresetCollection,bUseDependentPreset, PresetID, NodeID, IconRenderTarget);
		break;
	case EBIMValueScope::Module:
	case EBIMValueScope::Gap:
		bCaptureSuccess = SetIconMeshForModule(PresetCollection, bUseDependentPreset, PresetID, NodeID, IconRenderTarget);
		break;
	case EBIMValueScope::Mesh:
		bCaptureSuccess = SetIconMeshForMesh(PresetID, IconRenderTarget);
		break;
	case EBIMValueScope::Layer:
		if (bUseDependentPreset)
		{
			bCaptureSuccess = SetIconMeshForLayerPreset(PresetCollection, PresetID, IconRenderTarget);
		}
		else
		{
			bCaptureSuccess = SetIconMeshForLayerNodeID(PresetCollection, NodeID, IconRenderTarget);
		}
		break;
	}


	if (bCaptureSuccess)
	{
		// Always convert RenderTarget into texture2D
		// Dirty nodes or other temporary icons should not be cached
		if (bCanCache)
		{
			UThumbnailCacheManager::SaveThumbnailFromPresetKey(IconRenderTarget, PresetID, outTexture, this, true);
		}
		else
		{
			UThumbnailCacheManager::GetThumbnailFromTexture(IconRenderTarget, NodeID, outTexture, this);
		}

		if (outTexture)
		{
			UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconMaterial, this);
			dynMat->SetTextureParameterValue(MaterialIconTextureParamName, outTexture);
			OutMaterial = dynMat;
			return true;
		}
	}
	return false;
}

bool ADynamicIconGenerator::GetSavedIconFromPreset(const FGuid& PresetID, UTexture2D*& OutTexture)
{
	OutTexture = UThumbnailCacheManager::GetCachedThumbnailFromPresetKey(PresetID, this);
	return OutTexture != nullptr;
}

UMaterialInterface* ADynamicIconGenerator::CreateMaterialForIconTexture(UTexture2D* InTexture)
{
	UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconMaterial, this);
	dynMat->SetTextureParameterValue(MaterialIconTextureParamName, InTexture);
	return dynMat;
}

bool ADynamicIconGenerator::SetIconMeshForAssemblyType(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex, bool bFromRootNode)
{
	switch (Assembly.ObjectType)
	{
	case EObjectType::OTWallSegment:
	case EObjectType::OTFinish:
	case EObjectType::OTRailSegment:
	case EObjectType::OTSystemPanel:
		return SetIconMeshForWallAssembly(Assembly, InRenderTarget);
	case EObjectType::OTFloorSegment:
	case EObjectType::OTRoofFace:
	case EObjectType::OTCountertop:
	case EObjectType::OTCeiling:
		return SetIconMeshForFloorAssembly(Assembly, InRenderTarget);
	case EObjectType::OTDoor:
	case EObjectType::OTWindow:
		return SetIconMeshForPortalAssembly(Assembly, InRenderTarget, PartIndex);
	case EObjectType::OTStructureLine:
	case EObjectType::OTTrim:
	case EObjectType::OTMullion:
		return SetIconMeshForTrimAssembly(Assembly, InRenderTarget);
	case EObjectType::OTCabinet:
		return SetIconMeshForCabinetAssembly(Assembly, InRenderTarget, PartIndex, bFromRootNode);
	case EObjectType::OTFurniture:
		return SetIconMeshForFFEAssembly(Assembly, InRenderTarget);
	case  EObjectType::OTStaircase:
		return SetIconMeshForStairAssembly(Assembly, InRenderTarget, 
			(PartIndex == 0 || PartIndex == StairLayerTreadAssemblyPartIndex),
			(PartIndex == 0 || PartIndex == StairLayerRiserAssemblyPartIndex));
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForWallAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Setup for actor to create meshes from assembly
	// TODO: Investigate component bounding box issue when location isn't in FVector::zero
	IconDynamicMeshActor->SetActorLocation(FVector::ZeroVector);
	FVector p1(WallLength * 0.5f, 0.f, 0.f);
	FVector p2(WallLength * -0.5f, 0.f, 0.f);
	FVector p3(WallLength * -0.5f, 0.f, WallHeight);
	FVector p4(WallLength * 0.5f, 0.f, WallHeight);
	TArray<FVector> planePoints = { p1, p2, p3, p4};
	TArray<FPolyHole3D> holes;

	IconDynamicMeshActor->CreateBasicLayerDefs(planePoints, FVector::ZeroVector, holes, Assembly, 0.5f, FVector::ZeroVector);
	IconDynamicMeshActor->Assembly = Assembly;
	IconDynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);

	// Slice each layer, assuming each layer is successfully made
	for (int32 i = 0; i < Assembly.Layers.Num(); ++i)
	{
		if (IconDynamicMeshActor->ProceduralSubLayers[i])
		{
			FVector sliceLocation, sliceNormal;
			GetWallSliceLocationNormal(i, Assembly.Layers.Num(), p2, p1, WallHeight, sliceLocation, sliceNormal);
			UProceduralMeshComponent* otherHalfProcMesh;
			UKismetProceduralMeshLibrary::SliceProceduralMesh(
				IconDynamicMeshActor->ProceduralSubLayers[i],
				sliceLocation,
				sliceNormal,
				false,
				otherHalfProcMesh,
				EProcMeshSliceCapOption::UseLastSectionForCap,
				IconDynamicMeshActor->ProceduralSubLayers[i]->GetMaterial(0));
		}
	}
	IconDynamicMeshActor->SetActorRelativeTransform(FTransform::Identity);

	// Step 2: Set camera transform and actor for capture
	FTransform originalCaptureCompTransform = SceneCaptureComp->GetRelativeTransform();
	SetCaptureCompTransformForCapture(IconDynamicMeshActor, WallIconScaleFactor, true);
	SetIconDynamicMeshLayersForCapture(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Revert capture comp to its original transform, and actor to its original state
	SceneCaptureComp->SetRelativeTransform(originalCaptureCompTransform);
	SetIconDynamicMeshLayersForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForFloorAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Setup for actor to create meshes from assembly
	// TODO: Investigate component bounding box issue when location isn't in FVector::zero
	IconDynamicMeshActor->SetActorLocation(FVector::ZeroVector);
	FVector p1(FloorLength * -0.5f, FloorDepth * 0.5f, 0.f);
	FVector p2(FloorLength * 0.5f, FloorDepth * 0.5f, 0.f);
	FVector p3(FloorLength * 0.5f, FloorDepth * -0.5f, 0.f);
	FVector p4(FloorLength * -0.5f, FloorDepth * -0.5f, 0.f);
	TArray<FVector> planePoints = { p1, p2, p3, p4 };
	TArray<FPolyHole3D> holes;

	IconDynamicMeshActor->CreateBasicLayerDefs(planePoints, FVector::ZeroVector, holes, Assembly, 1.f, FVector::ZeroVector);
	IconDynamicMeshActor->Assembly = Assembly;
	IconDynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);

	// Slice each layer, assuming each layer is successfully made
	for (int32 i = 0; i < Assembly.Layers.Num(); ++i)
	{
		if (IconDynamicMeshActor->ProceduralSubLayers[i])
		{
			FVector sliceLocation, sliceNormal;
			bool bSliced;
			float layerRatio = 1.f / Assembly.Layers.Num();
			FVector sliceStart = p4 + layerRatio * (p3 - p4);
			FVector sliceEnd = p2 + layerRatio * (p1 - p2);

			GetFloorSliceLocationNormal(i, Assembly.Layers.Num(), sliceStart, sliceEnd, 0.f, sliceLocation, sliceNormal, bSliced);
			if (bSliced)
			{
				UProceduralMeshComponent* otherHalfProcMesh;
				UKismetProceduralMeshLibrary::SliceProceduralMesh(
					IconDynamicMeshActor->ProceduralSubLayers[i],
					sliceLocation,
					sliceNormal,
					false,
					otherHalfProcMesh,
					EProcMeshSliceCapOption::UseLastSectionForCap,
					IconDynamicMeshActor->ProceduralSubLayers[i]->GetMaterial(0));
			}
		}
	}
	IconDynamicMeshActor->SetActorRelativeTransform(FTransform::Identity);

	// Step 2: Set camera transform and actor for capture
	FTransform originalCaptureCompTransform = SceneCaptureComp->GetRelativeTransform();
	SetCaptureCompTransformForCapture(IconDynamicMeshActor, FloorIconScaleFactor, true);
	SetIconDynamicMeshLayersForCapture(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Revert capture comp to its original transform, and actor to its original state
	SceneCaptureComp->SetRelativeTransform(originalCaptureCompTransform);
	SetIconDynamicMeshLayersForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForPortalAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex)
{
	// Step 1: Setup for actor to create meshes from assembly
	// TODO: Check door not created with proper extent unless location is set to zero during MakeFromAssembly
	IconCompoundMeshActor->SetActorTransform(FTransform::Identity);
	IconCompoundMeshActor->MakeFromAssemblyPart(Assembly, PartIndex, FVector::OneVector, false, true);
	IconCompoundMeshActor->SetActorRelativeTransform(FTransform::Identity);

	// Step 2: Set camera transform and actor for capture
	FTransform originalCaptureCompTransform = SceneCaptureComp->GetRelativeTransform();
	SetCaptureCompTransformForCapture(IconCompoundMeshActor, PortalIconScaleFactor, true);
	SetIconCompoundMeshActorForCapture(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Revert capture comp to its original transform, and actor to its original state
	SceneCaptureComp->SetRelativeTransform(originalCaptureCompTransform);
	SetIconCompoundMeshActorForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForCabinetAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex, bool bIncludeCabinetBoxMesh)
{
	// Now that we have a cabinet assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a fake cabinet for icon generation the same way that AMOICabinet does.

	// Get the material for the cabinet
	FArchitecturalMaterial materialData;
	if (ensure(Assembly.Extrusions.Num() == 1))
	{
		materialData = Assembly.Extrusions[0].Material;
	}

	// Get the cabinet dimensions
	FModumateUnitValue cabinetWidthUnit(CabinetDimension.X, EModumateUnitType::WorldInches);
	FModumateUnitValue cabinetHeightUnit(CabinetDimension.Z, EModumateUnitType::WorldInches);
	FModumateUnitValue cabinetDepthUnit(CabinetDimension.Y, EModumateUnitType::WorldInches);

	float cabinetWidth = cabinetWidthUnit.AsWorldCentimeters();
	float cabinetHeight = cabinetHeightUnit.AsWorldCentimeters();
	float cabinetDepth = cabinetDepthUnit.AsWorldCentimeters();
	FVector cabinetExtents(cabinetWidth, cabinetDepth, cabinetHeight);

	// Make the cabinet base + box geometry
	FVector captureOrigin = GetActorLocation();
	TArray<FVector> cabinetBasePoints = {
		captureOrigin + 0.5f * FVector(-cabinetExtents.X, -cabinetExtents.Y, -cabinetExtents.Z),
		captureOrigin + 0.5f * FVector(cabinetExtents.X, -cabinetExtents.Y, -cabinetExtents.Z),
		captureOrigin + 0.5f * FVector(cabinetExtents.X, cabinetExtents.Y, -cabinetExtents.Z),
		captureOrigin + 0.5f * FVector(-cabinetExtents.X, cabinetExtents.Y, -cabinetExtents.Z),
	};
	FVector extrusionDelta = cabinetHeight * FVector::UpVector;

	// Step 1: Setup for actor to create meshes from assembly
	bool bFaceValid;
	AMOICabinet::UpdateCabinetActors(Assembly, cabinetBasePoints, extrusionDelta, 2, false, true, true, IconDynamicMeshActor, IconCompoundMeshActor, bFaceValid, PartIndex, bIncludeCabinetBoxMesh);

	// Step 2: Set camera transform and actor for capture
	FTransform originalCaptureCompTransform = SceneCaptureComp->GetRelativeTransform();
	if (bIncludeCabinetBoxMesh)
	{
		SetCaptureCompTransformForCapture(IconDynamicMeshActor, CabinetScaleFactor, true);
	}
	else
	{
		SetCaptureCompTransformForCapture(IconCompoundMeshActor, CabinetScaleFactor, true);
	}
	SetIconCompoundMeshActorForCapture(true);
	SetIconDynamicMeshLayersForCapture(bIncludeCabinetBoxMesh);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Revert capture comp to its original transform, and actor to its original state
	SceneCaptureComp->SetRelativeTransform(originalCaptureCompTransform);
	IconCompoundMeshActor->SetActorRelativeTransform(FTransform::Identity);
	IconDynamicMeshActor->SetActorRelativeTransform(FTransform::Identity);
	SetIconCompoundMeshActorForCapture(false);
	SetIconDynamicMeshLayersForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForTrimAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Setup for from assembly
	FVector meshStartPos = FVector(TrimLength * 0.5f, 0.f, 0.f);
	FVector meshEndPos = FVector(TrimLength * -0.5f, 0.f, 0.f);

	FVector meshNormal = Assembly.ObjectType == EObjectType::OTStructureLine ? FVector::LeftVector : FVector::RightVector;
	FVector meshUp = Assembly.ObjectType == EObjectType::OTStructureLine ? FVector::DownVector : FVector::UpVector;

	FVector2D extensions = FVector2D::ZeroVector;

	IconDynamicMeshActor->SetupExtrudedPolyGeometry(Assembly, meshStartPos, meshEndPos,
		meshUp, meshNormal, FDimensionOffset::Centered, FDimensionOffset::Centered, extensions, FVector::OneVector, true, false);
	IconDynamicMeshActor->SetActorRelativeTransform(FTransform::Identity);

	// Step 2: Set camera transform and actor for capture
	FTransform originalCaptureCompTransform = SceneCaptureComp->GetRelativeTransform();
	SetCaptureCompTransformForCapture(IconDynamicMeshActor, TrimIconScaleFactor, true);
	SetIconDynamicMeshMeshCompForCapture(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Revert capture comp to its original transform, and actor to its original state
	SceneCaptureComp->SetRelativeTransform(originalCaptureCompTransform);
	SetIconDynamicMeshMeshCompForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForFFEAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Generate model
	////////////////////////////////////////////////////////////////////
	TArray<UStaticMesh*> meshes;
	for (int32 i = 0; i < Assembly.Parts.Num(); ++i)
	{
		// The first part in a multi-part FFE is the root part and has no mesh
		if (Assembly.Parts[i].Mesh.EngineMesh.IsValid())
		{
			meshes.Add(Assembly.Parts[i].Mesh.EngineMesh.Get());
		}
	}
	if (meshes.Num() == 0)
	{
		return false;
	}
	UStaticMesh* mesh = meshes[0];
	////////////////////////////////////////////////////////////////////

	IconStaticMesh->SetStaticMesh(mesh);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	FVector meshScale = ((FFEIconScaleFactor / mesh->GetBounds().SphereRadius) * FVector::OneVector);
	FVector meshLocation = (mesh->GetBounds().Origin * meshScale) * -1.f;

	IconStaticMesh->SetRelativeLocation(meshLocation);
	IconStaticMesh->SetRelativeScale3D(meshScale);

	SetComponentForIconCapture(IconStaticMesh, true);
	IconStaticMesh->SetVisibility(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconStaticMesh->ResetRelativeTransform();
	SetComponentForIconCapture(IconStaticMesh, false);
	IconStaticMesh->SetVisibility(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForStairAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, bool bMakeTread, bool bMakeRiser)
{
	// Step 1: Setup stair mesh
	/////////////////////////////////////////////////////////////////////////////////////////////

	// Note: This should be similar to AMOIStaircase::SetupDynamicGeometry() in Stairs.cpp
	
	// Instead of MOI as planeParent, icon uses specific dimension for stair model
	TArray<FVector> runPlanePoints;

	// Tread 'depth' is horizontal run from nose to nose.
	float goalTreadDepth = Assembly.TreadDepthCentimeters;

	// If rendering just treads or just risers, only draw one stair
	FVector stairDim = (bMakeTread && bMakeRiser) ? 
		StairDimension :
		// Cut depth in half to zoom in a bit
		FVector(goalTreadDepth, StairDimension.Y*0.5f, goalTreadDepth);

	runPlanePoints.Add(FVector::ZeroVector);
	runPlanePoints.Add(FVector(0.f, stairDim.Y, 0.f));
	runPlanePoints.Add(FVector(stairDim.X, stairDim.Y, stairDim.Z));
	runPlanePoints.Add(FVector(stairDim.X, 0.f, stairDim.Z));

	// Calculate the polygons that make up the outer surfaces of each tread and riser of the linear stair run
	float stepRun, stepRise;
	bool bCachedUseRisers = Assembly.RiserLayers.Num() != 0;
	bool bCachedStartRiser = true;
	bool bCachedEndRiser = bMakeTread; // If we're just showing the riser, show only one
	TArray<TArray<FVector>> cachedTreadPolys;
	TArray<TArray<FVector>> cachedRiserPolys;

	FVector runDir(ForceInitToZero), stairOrigin(ForceInitToZero);
	if (!Modumate::FStairStatics::CalculateLinearRunPolysFromPlane(
		runPlanePoints, goalTreadDepth, bCachedUseRisers, bCachedStartRiser, bCachedEndRiser,
		stepRun, stepRise, runDir, stairOrigin,
		cachedTreadPolys, cachedRiserPolys))
	{
		return false;
	}

	TArray<FLayerGeomDef> treadLayers;
	TArray<FLayerGeomDef> riserLayers;
	FCachedLayerDimsByType cachedTreadDims;
	FCachedLayerDimsByType cachedRiserDims;
	TArray<FVector> cachedRiserNormals;

	// Empirically derived overlap.
	// TODO: put in assembly spec.
	static constexpr float openStairsOverhang = 2.0f * UModumateDimensionStatics::InchesToCentimeters;

	const float totalTreadThickness = cachedTreadDims.TotalUnfinishedWidth;
	const float totalRiserThickness = bCachedUseRisers ? cachedRiserDims.TotalUnfinishedWidth : openStairsOverhang;

	Modumate::FStairStatics::CalculateSetupStairPolysParams(
		Assembly,
		totalTreadThickness, totalRiserThickness, runDir,
		cachedTreadPolys, cachedRiserPolys,
		cachedRiserNormals, treadLayers, riserLayers);

	// Set up the triangulated staircase mesh by extruding each tread and riser polygon
	if (!bMakeTread)
	{
		treadLayers.Empty();
	}
	if (!bMakeRiser)
	{
		riserLayers.Empty();
	}
	bool bStairSuccess = IconDynamicMeshActor->SetupStairPolys(stairOrigin, cachedTreadPolys, cachedRiserPolys, cachedRiserNormals, treadLayers, riserLayers,
		Assembly);

	/////////////////////////////////////////////////////////////////////////////////////////////
	if (!bStairSuccess)
	{
		return false;
	}
	IconDynamicMeshActor->SetActorRelativeTransform(FTransform::Identity);

	// Step 2: Set camera transform and actor for capture
	FTransform originalCaptureCompTransform = SceneCaptureComp->GetRelativeTransform();
	SetCaptureCompTransformForCapture(IconDynamicMeshActor, StairIconScaleFactor, true);
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, true);
	IconDynamicMeshActor->Mesh->SetVisibility(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Revert capture comp to its original transform, and actor to its original state
	SceneCaptureComp->SetRelativeTransform(originalCaptureCompTransform);
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, false);
	IconDynamicMeshActor->Mesh->SetVisibility(false);
	IconDynamicMeshActor->Mesh->ClearAllMeshSections(); // Procedural mesh with multiple sections should be cleared out

	return true;
}

bool ADynamicIconGenerator::SetIconFromTextureAsset(const FGuid& PresetID, UMaterialInterface*& OutMaterial)
{
	const FStaticIconTexture* staticIcon = Gamemode->ObjectDatabase->GetStaticIconTextureByGUID(PresetID);
	if (staticIcon != nullptr && staticIcon->IsValid())
	{
		UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconMaterial, this);
		dynMat->SetTextureParameterValue(MaterialIconTextureParamName, staticIcon->Texture.Get());
		OutMaterial = dynMat;
		return true;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForRawMaterial(const FGuid& MaterialKey, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get material
	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByGUID(MaterialKey);
	if (!aMat->IsValid())
	{
		return false;
	}
	IconSphereMesh->SetMaterial(0, aMat->EngineMaterial.Get());

	// Step 2: Setup mesh for material
	SetComponentForIconCapture(IconSphereMesh, true);
	IconSphereMesh->SetVisibility(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	SetComponentForIconCapture(IconSphereMesh, false);
	IconSphereMesh->SetVisibility(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForProfile(const FGuid& ProfileKey, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get profile
	const FSimpleMeshRef* meshRef = Gamemode->ObjectDatabase->GetSimpleMeshByGUID(ProfileKey);
	if (!meshRef)
	{
		return false;
	}

	// Add temp assembly that uses the profile
	FBIMAssemblySpec obAsm;
	FBIMExtrusionSpec obAsmExtrusion;
	obAsmExtrusion.SimpleMeshes.Add(*meshRef);
	obAsm.Extrusions.Add(obAsmExtrusion);

	// Geometry params for setting up dynamic mesh actor
	FVector meshStartPos = FVector(1.f, 0.f, 0.f);
	FVector meshEndPos = FVector(-1.f, 0.f, 0.f);
	// Profile assets orientation is different in BeamColumns vs trim
	bool asBeamProfile = Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly.ObjectType == EObjectType::OTStructureLine;
	FVector meshNormal = asBeamProfile ? FVector::LeftVector : FVector::RightVector;
	FVector meshUp = asBeamProfile ? FVector::DownVector : FVector::UpVector;

	IconDynamicMeshActor->SetupExtrudedPolyGeometry(obAsm, meshStartPos, meshEndPos,
		meshUp, meshNormal, FDimensionOffset::Centered, FDimensionOffset::Centered, FVector2D::ZeroVector, FVector::OneVector, true, false);
	IconDynamicMeshActor->SetActorRelativeTransform(FTransform::Identity);

	// Step 2: Set camera transform and actor for capture
	// For 2D profile icon, rotation the capture comp to point directly at profile mesh
	FTransform originalCaptureCompTransform = SceneCaptureComp->GetRelativeTransform();
	SceneCaptureComp->SetWorldTransform(FTransform::Identity);
	SetCaptureCompTransformForCapture(IconDynamicMeshActor, ProfileIconScaleFactor, true);
	SetIconDynamicMeshMeshCompForCapture(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Revert capture comp to its original transform, and actor to its original state
	SceneCaptureComp->SetRelativeTransform(originalCaptureCompTransform);
	SetIconDynamicMeshMeshCompForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForMesh(const FGuid& MeshKey, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get mesh from key
	const FArchitecturalMesh* aMesh = Gamemode->ObjectDatabase->GetArchitecturalMeshByGUID(MeshKey);
	if (!aMesh->EngineMesh.IsValid())
	{
		return false;
	}
	UStaticMesh* mesh = aMesh->EngineMesh.Get();
	IconStaticMesh->SetStaticMesh(mesh);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	FVector meshScale = ((FFEIconScaleFactor / mesh->GetBounds().SphereRadius) * FVector::OneVector);
	FVector meshLocation = (mesh->GetBounds().Origin * meshScale) * -1.f;

	IconStaticMesh->SetRelativeLocation(meshLocation);
	IconStaticMesh->SetRelativeScale3D(meshScale);
	// Mesh render does not have material, capture render with basic material instead
	for (int32 i = 0; i < IconStaticMesh->GetNumMaterials(); ++i)
	{
		IconStaticMesh->OverrideMaterials.Add(CustomMaterialBase);
	}

	SetComponentForIconCapture(IconStaticMesh, true);
	IconStaticMesh->SetVisibility(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	// Restore from override material
	IconStaticMesh->OverrideMaterials.Empty();
	IconStaticMesh->ResetRelativeTransform();
	SetComponentForIconCapture(IconStaticMesh, false);
	IconStaticMesh->SetVisibility(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForPart(const FBIMPresetCollectionProxy& PresetCollection,bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get params needed to make parts
	FGuid rawMaterialKey;
	FString colorHexValue = TEXT("FFFFFF");
	FGuid meshKey;

	// Step 2: Should this icon be using its dependent presets, or use preset values from its children node?
	if (UseDependentPreset)
	{
		const FBIMPresetInstance* preset = PresetCollection.PresetFromGUID(PresetID);
		preset->Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		preset->Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
		// Get mesh key from child preset
		for (const auto& curChild : preset->ChildPresets)
		{
			const FBIMPresetInstance* curChildPreset = PresetCollection.PresetFromGUID(curChild.PresetGUID);
			if (curChildPreset->NodeScope == EBIMValueScope::Mesh)
			{
				meshKey = curChild.PresetGUID;
			}
		}
	}
	else
	{
		const FBIMPresetEditorNodeSharedPtr inst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
		// Get mesh key from child node
		TArray<FBIMPresetEditorNodeSharedPtr> childrenNodes;
		inst->GatherAllChildNodes(childrenNodes);
		for (const auto& child : childrenNodes)
		{
			const FBIMPresetInstance* preset = PresetCollection.PresetFromGUID(child->Preset.GUID);
			if (preset->NodeScope == EBIMValueScope::Mesh)
			{
				meshKey = child->Preset.GUID;
			}
		}
	}

	// Step 3: Get assets from key, and size from dimension
	FVector vSize = FVector::OneVector;

	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByGUID(rawMaterialKey);
	const FArchitecturalMesh* aMesh = Gamemode->ObjectDatabase->GetArchitecturalMeshByGUID(meshKey);

	// Step 4: Set assets
	if (aMat != nullptr && aMesh != nullptr && aMesh->EngineMesh.IsValid())
	{
		UStaticMesh* mesh = aMesh->EngineMesh.Get();
		IconStaticMesh->SetStaticMesh(mesh);

		// Override material
		// TODO: Meshes that have multiple material channels
		auto* dynMat = IconStaticMesh->CreateDynamicMaterialInstance(0, aMat->EngineMaterial.Get());
		dynMat->SetVectorParameterValue(MaterialColorParamName, FColor::FromHex(colorHexValue));
		for (int32 i = 0; i < IconStaticMesh->GetNumMaterials(); ++i)
		{
			IconStaticMesh->OverrideMaterials.Add(dynMat);
		}

		// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
		FVector meshScale = ((FFEIconScaleFactor / mesh->GetBounds().SphereRadius) * FVector::OneVector);
		FVector meshLocation = (mesh->GetBounds().Origin * meshScale) * -1.f;
		IconStaticMesh->SetRelativeLocation(meshLocation);
		IconStaticMesh->SetRelativeScale3D(meshScale);

		SetComponentForIconCapture(IconStaticMesh, true);
		IconStaticMesh->SetVisibility(true);
		SceneCaptureComp->TextureTarget = InRenderTarget;
		SceneCaptureComp->CaptureScene();

		// Step 3: Cleanup
		// Restore from override material
		IconStaticMesh->OverrideMaterials.Empty();
		IconStaticMesh->ResetRelativeTransform();
		SetComponentForIconCapture(IconStaticMesh, false);
		IconStaticMesh->SetVisibility(false);
		return true;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForMaterial(const FBIMPresetCollectionProxy& PresetCollection,bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get params needed to make material icon
	FGuid rawMaterialKey;
	FString colorHexValue = TEXT("FFFFFF");

	// Step 2: Should this icon be using its dependent presets, or use preset values from its children node?
	if (UseDependentPreset)
	{
		const FBIMPresetInstance* preset = PresetCollection.PresetFromGUID(PresetID);
		preset->Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		preset->Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
	}
	else
	{
		const FBIMPresetEditorNodeSharedPtr inst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
	}

	// Step 3: Get assets from key
	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByGUID(rawMaterialKey);

	// Step 4: Set assets
	if (aMat != nullptr)
	{
		auto* dynMat = IconSphereMesh->CreateDynamicMaterialInstance(0, aMat->EngineMaterial.Get());
		dynMat->SetVectorParameterValue(MaterialColorParamName, FColor::FromHex(colorHexValue));
		IconSphereMesh->SetMaterial(0, dynMat);

		// Step 5: Capture icon
		SetComponentForIconCapture(IconSphereMesh, true);
		IconSphereMesh->SetVisibility(true);
		SceneCaptureComp->TextureTarget = InRenderTarget;
		SceneCaptureComp->CaptureScene();

		// Step 6: Cleanup
		SetComponentForIconCapture(IconSphereMesh, false);
		IconSphereMesh->SetVisibility(false);
		return true;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForModule(const FBIMPresetCollectionProxy& PresetCollection, bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get params needed to make module icon
	FGuid rawMaterialKey;
	FString colorHexValue = TEXT("FFFFFF");

	FModumateUnitValue 
		width = FModumateUnitValue::WorldCentimeters(0), 
		length = FModumateUnitValue::WorldCentimeters(0),
		depth = FModumateUnitValue::WorldCentimeters(0),
		height = FModumateUnitValue::WorldCentimeters(0);

	// Step 2: Should this icon be using its dependent presets, or use preset values from its children node?
	if (UseDependentPreset)
	{
		const FBIMPresetInstance* preset = PresetCollection.PresetFromGUID(PresetID);
		preset->Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		preset->Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
		preset->Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, width);
		preset->Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, length);
		preset->Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, depth);
		preset->Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Height, height);
	}
	else
	{
		const FBIMPresetEditorNodeSharedPtr inst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, width);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, length);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, depth);
		inst->Preset.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Height, height);
	}

	// Step 3: Get assets from key, and size from dimension
	FVector vSize = FVector::OneVector;

	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByGUID(rawMaterialKey);
	if (width.AsWorldCentimeters() > 0)
	{
		vSize.Y = width.AsWorldCentimeters();
	}
	if (length.AsWorldCentimeters() > 0)
	{
		vSize.X = length.AsWorldCentimeters();
	}
	if (depth.AsWorldCentimeters() > 0)
	{
		vSize.Y = depth.AsWorldCentimeters();
	}
	if (height.AsWorldCentimeters() > 0)
	{
		vSize.Z = height.AsWorldCentimeters();
	}

	// Step 4: Set assets
	if (aMat != nullptr)
	{
		auto* dynMat = IconCubeMesh->CreateDynamicMaterialInstance(0, aMat->EngineMaterial.Get());
		dynMat->SetVectorParameterValue(MaterialColorParamName, FColor::FromHex(colorHexValue));
		IconCubeMesh->SetMaterial(0, dynMat);
		FVector normalizedSize = vSize.GetSafeNormal() * 0.5f;
		IconCubeMesh->SetWorldScale3D(normalizedSize);

		// Step 5: Capture icon
		SetComponentForIconCapture(IconCubeMesh, true);
		IconCubeMesh->SetVisibility(true);
		SceneCaptureComp->TextureTarget = InRenderTarget;
		SceneCaptureComp->CaptureScene();

		// Step 6: Cleanup
		IconCubeMesh->ResetRelativeTransform();
		SetComponentForIconCapture(IconCubeMesh, false);
		IconCubeMesh->SetVisibility(false);
		return true;
	}

	return false;
}

bool ADynamicIconGenerator::SetIconMeshForLayerNodeID(const FBIMPresetCollectionProxy& PresetCollection, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	FBIMAssemblySpec assembly;
	Controller->EditModelUserWidget->BIMDesigner->InstancePool.CreateAssemblyFromLayerNode(*Gamemode->ObjectDatabase, NodeID, assembly);
	return SetIconMeshForWallAssembly(assembly, InRenderTarget);
}

bool ADynamicIconGenerator::SetIconMeshForLayerPreset(const FBIMPresetCollectionProxy& PresetCollection, const FGuid& PresetID, UTextureRenderTarget2D* InRenderTarget)
{
	FBIMAssemblySpec assembly;
	EObjectType objType = EObjectType::OTWallSegment; // TODO: Get object type, default to wall for now
	FBIMPresetCollectionProxy mutableCollection(PresetCollection);
	EBIMResult result = mutableCollection.CreateAssemblyFromLayerPreset(*Gamemode->ObjectDatabase, PresetID, objType, assembly);
	return SetIconMeshForWallAssembly(assembly, InRenderTarget);
}

bool ADynamicIconGenerator::SetIconFromColor(const FBIMKey& ColorHex, UMaterialInterface*& OutMaterial)
{
	FLinearColor iconColor = FColor::FromHex(ColorHex.ToString());
	UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconColorMaterial, this);
	dynMat->SetVectorParameterValue(MaterialColorParamName, iconColor);
	OutMaterial = dynMat;
	return true;
}

void ADynamicIconGenerator::GetWallSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& Cp1, const FVector& Cp2, float Height, FVector& OutLocation, FVector& OutNormal)
{
	FVector endPt = FVector(Cp1.X, Cp1.Y, Cp1.Z + Height);
	float sliceLocationAlpha = (NumberOfLayers - CurrentLayer + 1.f) / (NumberOfLayers + 1.f);
	OutNormal = (Cp2 - endPt).GetSafeNormal();
	OutLocation = Cp2 + sliceLocationAlpha * (endPt - Cp2);
}

void ADynamicIconGenerator::GetFloorSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& StartPt, const FVector& EndPt, float Height, FVector& OutLocation, FVector& OutNormal, bool& OutSliced)
{
	int32 sliceId = NumberOfLayers - 1 - CurrentLayer;
	if (sliceId == 0)
	{
		OutLocation = FVector::ZeroVector;
		OutNormal = FVector::ZeroVector;
		OutSliced = false;
	}
	else
	{
		float sliceLocationAlpha = sliceId / (NumberOfLayers + 1.f);
		OutLocation = EndPt + sliceLocationAlpha * (StartPt - EndPt);
		OutNormal = FVector(-.5f, -.55f, 0.f);
		OutSliced = true;
	}
}

bool ADynamicIconGenerator::SetComponentForIconCapture(UPrimitiveComponent* Comp, bool CanCapture)
{
	if (Comp)
	{
		if (CanCapture)
		{
			Comp->SetBoundsScale(1000);
			Comp->LightingChannels.bChannel0 = false;
			Comp->LightingChannels.bChannel2 = true;
			Comp->SetRenderCustomDepth(true);
			Comp->CustomDepthStencilValue = 1;
		}
		else
		{
			Comp->SetBoundsScale(1);
		}

		return true;
	}
	return false;
}

void ADynamicIconGenerator::SetIconDynamicMeshLayersForCapture(bool Visible)
{
	for (auto& curLayer : IconDynamicMeshActor->ProceduralSubLayers)
	{
		SetComponentForIconCapture(curLayer, Visible);
		curLayer->SetVisibility(Visible);
	}
	if (!Visible)
	{
		IconDynamicMeshActor->ClearProceduralLayers();
	}
}

void ADynamicIconGenerator::SetIconDynamicMeshMeshCompForCapture(bool Visible)
{
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, Visible);
	IconDynamicMeshActor->Mesh->SetVisibility(Visible);
	if (!Visible)
	{
		IconDynamicMeshActor->Mesh->ClearAllMeshSections();
	}
}

void ADynamicIconGenerator::SetIconCompoundMeshActorForCapture(bool Visible)
{
	TArray<UPrimitiveComponent*> comps;
	IconCompoundMeshActor->GetComponents<UPrimitiveComponent>(comps);
	for (auto& curComp : comps)
	{
		SetComponentForIconCapture(curComp, Visible);
	}
	IconCompoundMeshActor->SetActorHiddenInGame(!Visible);
}

void ADynamicIconGenerator::SetCaptureCompTransformForCapture(AActor* ActorToCapture, float SizeScale, bool OnlyCollidingComponents)
{
	FVector actorBoxOrigin, actorBoxExtents;
	ActorToCapture->GetActorBounds(OnlyCollidingComponents, actorBoxOrigin, actorBoxExtents);
	actorBoxExtents = actorBoxExtents * SizeScale;

	FSphere selectedBounds(actorBoxOrigin, actorBoxExtents.Size());
	FTransform camTransform = SceneCaptureComp->GetComponentTransform();
	FVector selectionViewOrigin = Controller->CalculateViewLocationForSphere(
		selectedBounds,
		camTransform.GetRotation().GetForwardVector(),
		1.f,
		SceneCaptureComp->FOVAngle);
	FTransform newTargetTransform;
	newTargetTransform.SetComponents(camTransform.GetRotation(), selectionViewOrigin, camTransform.GetScale3D());

	SceneCaptureComp->SetWorldTransform(newTargetTransform);
}

void ADynamicIconGenerator::UpdateCachedAssemblies(const FBIMPresetCollectionProxy& PresetCollection, const TArray<FGuid>& AsmKeys)
{
	for (auto& key : AsmKeys)
	{
		SetIconMeshForAssembly(PresetCollection, key, true);
	}
	Controller->EditModelUserWidget->RefreshAssemblyList();
}
