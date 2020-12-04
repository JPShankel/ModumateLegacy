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
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
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

	Gamemode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
	GameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
	Controller = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());

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
	ReleaseSavedRenderTarget();
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ADynamicIconGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool ADynamicIconGenerator::SetIconMeshForAssembly(const FBIMKey& AsmKey, UMaterialInterface*& OutMaterial)
{
	OutMaterial = nullptr;
	UTexture* iconTexture = nullptr;

	if (GetSavedIconFromPreset(AsmKey, iconTexture))
	{
		OutMaterial = CreateMaterialForIconTexture(AsmKey, iconTexture);
		return true;
	}

	const FBIMPresetInstance* childPreset = GameState->Document.PresetManager.CraftingNodePresets.Presets.Find(AsmKey);
	if (childPreset == nullptr)
	{
		return false;
	}
	const FPresetManager::FAssemblyDataCollection* db = GameState->Document.PresetManager.AssembliesByObjectType.Find(childPreset->ObjectType);
	if (db == nullptr)
	{
		return false;
	}
	const FBIMAssemblySpec* assembly = db->GetData(AsmKey);
	if (!assembly)
	{
		return false;
	}

	bool captureSuccess = SetIconMeshForAssemblyType(*assembly, IconRenderTarget);
	if (captureSuccess)
	{
		UTexture2D* outSavedTexture = nullptr;
		captureSuccess = UThumbnailCacheManager::SaveThumbnailFromPresetKey(IconRenderTarget, AsmKey, outSavedTexture, this);
		OutMaterial = CreateMaterialForIconTexture(AsmKey, outSavedTexture);
	}

	return captureSuccess;
}

bool ADynamicIconGenerator::SetIconMeshForBIMDesigner(bool UseDependentPreset, const FBIMKey& PresetID, UMaterialInterface*& OutMaterial, UTexture*& OutTexture, const FBIMEditorNodeIDType& NodeID)
{
	// TODO: Attempt to use cached icon first, make new if not available
	OutMaterial = nullptr;
	OutTexture = nullptr;
#if 0
	if (GetSavedIconFromPreset(PresetID, iconTexture))
	{
		// TODO: Make OutMaterial from texture
		return true;
	}
#endif

	const FPresetManager &presetManager = GameState->Document.PresetManager;
	const FBIMPresetInstance* preset = presetManager.CraftingNodePresets.Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return false;
	}

	// First go through the presets that don't need render capture for icon
	switch (preset->NodeScope)
	{
	case EBIMValueScope::Color: // Color is a material with dynamic color, skip render capture
		SetIconMeshForColor(PresetID, OutMaterial);
		return true;
	case EBIMValueScope::Dimension: // Dimension icon is a static material, skip render capture
		OutMaterial = IconDimensionMaterial;
		return true;
	case EBIMValueScope::Pattern:
		return SetIconFromTextureAsset(PresetID, OutMaterial);
	}

	// The following presets need render capture for icon
	UTextureRenderTarget2D* renderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), RenderTargetSize, RenderTargetSize, ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor::Black, true);
	bool captureSuccess = false;
	switch (preset->NodeScope)
	{
	case EBIMValueScope::Assembly:
		captureSuccess = SetIconMeshForAssemblyType(Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly, renderTarget);
		break;
	case EBIMValueScope::RawMaterial:
		captureSuccess = SetIconMeshForRawMaterial(PresetID, renderTarget);
		break;
	case EBIMValueScope::Profile:
		captureSuccess = SetIconMeshForProfile(PresetID, renderTarget);
		break;
	case EBIMValueScope::Material:
		captureSuccess = SetIconMeshForMaterial(UseDependentPreset, PresetID, NodeID, renderTarget);
		break;
	case EBIMValueScope::Module:
	case EBIMValueScope::Gap:
		captureSuccess = SetIconMeshForModule(UseDependentPreset, PresetID, NodeID, renderTarget);
		break;
	case EBIMValueScope::Mesh:
		captureSuccess = SetIconMeshForMesh(PresetID, renderTarget);
		break;
	case EBIMValueScope::Part:
		captureSuccess = SetIconMeshForPart(UseDependentPreset, PresetID, NodeID, renderTarget);
		break;
	case EBIMValueScope::Layer:
		if (UseDependentPreset)
		{
			captureSuccess = SetIconMeshForLayerPreset(PresetID, renderTarget);
		}
		else
		{
			captureSuccess = SetIconMeshForLayerNodeID(NodeID, renderTarget);
		}

		break;
	}
	
	if (captureSuccess)
	{
		// TODO: Save texture cache, release RT resource
		BIMKeyToRenderTarget.Add(renderTarget);

		OutTexture = renderTarget;
		UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconMaterial, this);
		dynMat->SetTextureParameterValue(MaterialIconTextureParamName, OutTexture);
		OutMaterial = dynMat;
		return true;
	}
	else
	{
		renderTarget->ReleaseResource();
		return false;
	}
}

bool ADynamicIconGenerator::GetSavedIconFromPreset(const FBIMKey& PresetID, UTexture*& OutTexture)
{
	OutTexture = nullptr;
	const FPresetManager &presetManager = GameState->Document.PresetManager;
	const FBIMPresetInstance* preset = presetManager.CraftingNodePresets.Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return false;
	}

	if (preset->NodeScope == EBIMValueScope::Assembly)
	{
		OutTexture = UThumbnailCacheManager::GetCachedThumbnailFromPresetKey(PresetID, this);
	}
	else
	{
		// TODO: support cache for other preset scope
		return false;
	}

	return OutTexture != nullptr;
}

UMaterialInterface* ADynamicIconGenerator::CreateMaterialForIconTexture(const FBIMKey& PresetID, UTexture* InTexture)
{
	UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconMaterial, this);
	dynMat->SetTextureParameterValue(MaterialIconTextureParamName, InTexture);
	return dynMat;
}

bool ADynamicIconGenerator::SetIconMeshForAssemblyType(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
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
		return SetIconMeshForPortalAssembly(Assembly, InRenderTarget);
	case EObjectType::OTStructureLine:
	case EObjectType::OTTrim:
	case EObjectType::OTMullion:
		return SetIconMeshForTrimAssembly(Assembly, InRenderTarget);
	case EObjectType::OTCabinet:
		return SetIconMeshForCabinetAssembly(Assembly, InRenderTarget);
	case EObjectType::OTFurniture:
		return SetIconMeshForFFEAssembly(Assembly, InRenderTarget);
	case  EObjectType::OTStaircase:
		return SetIconMeshForStairAssembly(Assembly, InRenderTarget);
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForWallAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
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
	SetIconDynamicMeshLayersForCapture(true);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	FVector meshOrigin, meshExtent;
	IconDynamicMeshActor->GetActorBounds(true, meshOrigin, meshExtent, true);
	meshExtent.Normalize();
	FVector meshSize = FVector(meshExtent.Size()) * WallIconScaleFactor;
	FVector meshLocation = meshOrigin * meshExtent * -1.f;
	IconDynamicMeshActor->SetActorRelativeLocation(FVector(meshLocation.X, 0.f, meshLocation.Z * 0.5f));
	IconDynamicMeshActor->SetActorScale3D(meshSize);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconDynamicMeshLayersForCapture(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForFloorAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
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
	SetIconDynamicMeshLayersForCapture(true);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	FVector meshOrigin, meshExtent;
	IconDynamicMeshActor->GetActorBounds(true, meshOrigin, meshExtent, true);
	meshExtent.Normalize();
	FVector meshSize = FVector(meshExtent.Size()) * FloorIconScaleFactor;
	FVector meshLocation = meshOrigin * meshSize * -1.f;
	IconDynamicMeshActor->SetActorRelativeLocation(FVector(0.f, 0.f, meshLocation.Z));
	IconDynamicMeshActor->SetActorScale3D(meshSize);

	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconDynamicMeshLayersForCapture(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForPortalAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	IconCompoundMeshActor->MakeFromAssembly(Assembly, FVector::OneVector, false, true);

	// Step 1: Calculate and adjust model to fit inside the view of SceneCaptureComp
	// TODO: Some assemblies have native size = 0 
	FVector portalSize = Assembly.GetRiggedAssemblyNativeSize();
	float portalWidth = portalSize.X;
	float portalHeight = portalSize.Z;

	FVector meshSize = (PortalIconScaleFactor / FVector(portalWidth, 0.f, portalHeight).Size()) * FVector::OneVector;
	FVector meshLocation = (FVector(portalWidth, 0.f, portalHeight)) * meshSize * -0.5f;

	IconCompoundMeshActor->SetActorRelativeLocation(meshLocation);
	IconCompoundMeshActor->SetActorRelativeScale3D(meshSize);

	// Step 2: Set bound render to prevent mesh from being occluded by front mesh
	SetIconCompoundMeshActorForCapture(true);

	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconCompoundMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconCompoundMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconCompoundMeshActorForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForCabinetAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	// Now that we have a cabinet assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a fake cabinet for icon generation the same way that FMOICabinetImpl does.

	// Get the material for the cabinet
	FArchitecturalMaterial materialData;
	if (ensure(Assembly.Extrusions.Num() == 1))
	{
		materialData = Assembly.Extrusions[0].Material;
	}

	// Get the cabinet dimensions
	Units::FUnitValue cabinetWidthUnit(CabinetDimension.X, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetHeightUnit(CabinetDimension.Z, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetDepthUnit(CabinetDimension.Y, Units::EUnitType::WorldInches);

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

	// Step 2: Update the actors to show the current cabinet assembly
	bool bFaceValid;
	FMOICabinetImpl::UpdateCabinetActors(Assembly, cabinetBasePoints, extrusionDelta, 2, false, false, false, IconDynamicMeshActor, IconCompoundMeshActor, bFaceValid);

	// Step 3: Position the spring arm in order to capture the cabinet
	SetSpringArmDistanceForCapture(IconDynamicMeshActor, CabinetScaleFactor, false);

	// Set bound render to prevent mesh from being occluded by front mesh
	SetIconCompoundMeshActorForCapture(true);
	SetIconDynamicMeshLayersForCapture(true);

	// Step 4: Capture!
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 5: Cleanup
	ResetSpringArm();
	IconCompoundMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconCompoundMeshActor->SetActorScale3D(FVector::OneVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconCompoundMeshActorForCapture(false);
	SetIconDynamicMeshLayersForCapture(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForTrimAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	// Now that we have an assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a mesh for icon generation.
	FVector meshStartPos = FVector(TrimLength * 0.5f, 0.f, 0.f);
	FVector meshEndPos = FVector(TrimLength * -0.5f, 0.f, 0.f);

	FVector meshNormal = Assembly.ObjectType == EObjectType::OTStructureLine ? FVector::LeftVector : FVector::RightVector;
	FVector meshUp = Assembly.ObjectType == EObjectType::OTStructureLine ? FVector::DownVector : FVector::UpVector;

	FVector2D upperExtensions = FVector2D::ZeroVector;
	FVector2D outerExtensions = FVector2D::ZeroVector;

	IconDynamicMeshActor->SetupExtrudedPolyGeometry(Assembly, meshStartPos, meshEndPos,
		meshNormal, meshUp, upperExtensions, outerExtensions, FVector::OneVector, true, false);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	FVector meshExtent = IconDynamicMeshActor->Mesh->Bounds.BoxExtent;
	FVector meshOrigin = IconDynamicMeshActor->Mesh->Bounds.Origin;
	FVector meshSize = ((TrimIconScaleFactor / meshExtent.Size()) * FVector::OneVector);
	FVector meshLocation = meshOrigin * meshSize * -1.f;

	IconDynamicMeshActor->SetActorRelativeLocation(meshLocation);
	IconDynamicMeshActor->SetActorRelativeScale3D(meshSize);

	// Set bound render to prevent mesh from being occluded by front mesh
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, true);
	IconDynamicMeshActor->Mesh->SetVisibility(true);

	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, false);
	IconDynamicMeshActor->Mesh->SetVisibility(false);
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

bool ADynamicIconGenerator::SetIconMeshForStairAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Setup stair mesh
	/////////////////////////////////////////////////////////////////////////////////////////////

	// Note: This should be similar to FMOIStaircaseImpl::SetupDynamicGeometry() in Stairs.cpp
	
	// Instead of MOI as planeParent, icon uses specific dimension for stair model
	TArray<FVector> runPlanePoints;
	runPlanePoints.Add(FVector::ZeroVector);
	runPlanePoints.Add(FVector(0.f, StairDimension.Y, 0.f));
	runPlanePoints.Add(FVector(StairDimension.X, StairDimension.Y, StairDimension.Z));
	runPlanePoints.Add(FVector(StairDimension.X, 0.f, StairDimension.Z));

	// Tread 'depth' is horizontal run from nose to nose.
	float goalTreadDepth = Assembly.TreadDepth.AsWorldCentimeters();

	// Calculate the polygons that make up the outer surfaces of each tread and riser of the linear stair run
	float stepRun, stepRise;
	bool bCachedUseRisers = Assembly.RiserLayers.Num() != 0;
	bool bCachedStartRiser = true;
	bool bCachedEndRiser = true;
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
	static constexpr float openStairsOverhang = 2.0f * Modumate::InchesToCentimeters;

	const float totalTreadThickness = cachedTreadDims.TotalUnfinishedWidth;
	const float totalRiserThickness = bCachedUseRisers ? cachedRiserDims.TotalUnfinishedWidth : openStairsOverhang;

	Modumate::FStairStatics::CalculateSetupStairPolysParams(
		Assembly,
		totalTreadThickness, totalRiserThickness, runDir,
		cachedTreadPolys, cachedRiserPolys,
		cachedRiserNormals, treadLayers, riserLayers);

	// Set up the triangulated staircase mesh by extruding each tread and riser polygon
	bool bStairSuccess = IconDynamicMeshActor->SetupStairPolys(stairOrigin, cachedTreadPolys, cachedRiserPolys, cachedRiserNormals, treadLayers, riserLayers,
		Assembly);

	/////////////////////////////////////////////////////////////////////////////////////////////
	if (!bStairSuccess)
	{
		return false;
	}
	// Step 2: Position the spring arm in order to capture mesh
	// Reset stair relative location
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	SetSpringArmDistanceForCapture(IconDynamicMeshActor, StairIconScaleFactor, false);

	// Step 3: Capture
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, true);
	IconDynamicMeshActor->Mesh->SetVisibility(true);
	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 4: Cleanup
	ADynamicIconGenerator::ResetSpringArm();
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, false);
	IconDynamicMeshActor->Mesh->SetVisibility(false);
	IconDynamicMeshActor->Mesh->ClearAllMeshSections(); // Procedural mesh with multiple sections should be cleared out
	return true;
}

bool ADynamicIconGenerator::SetIconFromTextureAsset(const FBIMKey& PresetID, UMaterialInterface*& OutMaterial)
{
	const FStaticIconTexture* staticIcon = Gamemode->ObjectDatabase->GetStaticIconTextureByKey(PresetID);
	if (staticIcon != nullptr && staticIcon->IsValid())
	{
		UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconMaterial, this);
		dynMat->SetTextureParameterValue(MaterialIconTextureParamName, staticIcon->Texture.Get());
		OutMaterial = dynMat;
		return true;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForRawMaterial(const FBIMKey& MaterialKey, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get material
	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByKey(MaterialKey);
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

bool ADynamicIconGenerator::SetIconMeshForColor(const FBIMKey& ColorKey, UMaterialInterface*& OutMaterial)
{
	// Step 1: Get color
	const FCustomColor* customColor = Gamemode->ObjectDatabase->GetCustomColorByKey(ColorKey);
	if (customColor->IsValid())
	{
		UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(IconColorMaterial, this);
		dynMat->SetVectorParameterValue(MaterialColorParamName, customColor->Color);
		OutMaterial = dynMat;
		return true;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForProfile(const FBIMKey& ProfileKey, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get profile
	const FSimpleMeshRef* meshRef = Gamemode->ObjectDatabase->GetSimpleMeshByKey(ProfileKey);
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
		meshNormal, meshUp, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector::OneVector, true, false);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	FVector meshScale = ((ProfileIconScaleFactor / IconDynamicMeshActor->Mesh->Bounds.SphereRadius) * FVector::OneVector);
	FVector meshLocation = (IconDynamicMeshActor->Mesh->Bounds.Origin * meshScale) * -1.f;
	IconDynamicMeshActor->SetActorRelativeLocation(meshLocation);
	IconDynamicMeshActor->SetActorRelativeScale3D(meshScale);
	// For 2D profile icon, rotation the capture comp to point directly at profile mesh
	FRotator originalSpringArmRotation = SpringArm->GetRelativeRotation();
	SpringArm->SetRelativeRotation(FRotator::ZeroRotator);

	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, true);
	IconDynamicMeshActor->Mesh->SetVisibility(true);

	SceneCaptureComp->TextureTarget = InRenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	IconDynamicMeshActor->SetActorRotation(FRotator::ZeroRotator);
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, false);
	IconDynamicMeshActor->Mesh->SetVisibility(false);
	SpringArm->SetRelativeRotation(originalSpringArmRotation);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForMesh(const FBIMKey& MeshKey, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get mesh from key
	const FArchitecturalMesh* aMesh = Gamemode->ObjectDatabase->GetArchitecturalMeshByKey(MeshKey);
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

bool ADynamicIconGenerator::SetIconMeshForPart(bool UseDependentPreset, const FBIMKey& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get params needed to make parts
	FBIMKey rawMaterialKey;
	FString colorHexValue = TEXT("FFFFFF");
	FBIMKey meshKey;
	const FPresetManager &presetManager = GameState->Document.PresetManager;

	// Step 2: Should this icon be using its dependent presets, or use preset values from its children node?
	if (UseDependentPreset)
	{
		const FBIMPresetInstance* preset = presetManager.CraftingNodePresets.Presets.Find(PresetID);
		preset->Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		preset->Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
		// Get mesh key from child preset
		for (const auto& curChild : preset->ChildPresets)
		{
			const FBIMPresetInstance* curChildPreset = presetManager.CraftingNodePresets.Presets.Find(curChild.PresetID);
			if (curChildPreset->NodeScope == EBIMValueScope::Mesh)
			{
				meshKey = curChild.PresetID;
			}
		}
	}
	else
	{
		const FBIMPresetEditorNodeSharedPtr inst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
		// Get mesh key from child node
		TArray<FBIMPresetEditorNodeSharedPtr> childrenNodes;
		inst->GatherAllChildNodes(childrenNodes);
		for (const auto& child : childrenNodes)
		{
			const FBIMPresetInstance* preset = presetManager.CraftingNodePresets.Presets.Find(child->WorkingPresetCopy.PresetID);
			if (preset->NodeScope == EBIMValueScope::Mesh)
			{
				meshKey = child->WorkingPresetCopy.PresetID;
			}
		}
	}

	// Step 3: Get assets from key, and size from dimension
	FVector vSize = FVector::OneVector;

	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByKey(rawMaterialKey);
	const FArchitecturalMesh* aMesh = Gamemode->ObjectDatabase->GetArchitecturalMeshByKey(meshKey);

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

bool ADynamicIconGenerator::SetIconMeshForMaterial(bool UseDependentPreset, const FBIMKey& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get params needed to make material icon
	FBIMKey rawMaterialKey;
	FString colorHexValue = TEXT("FFFFFF");
	const FPresetManager &presetManager = GameState->Document.PresetManager;

	// Step 2: Should this icon be using its dependent presets, or use preset values from its children node?
	if (UseDependentPreset)
	{
		const FBIMPresetInstance* preset = presetManager.CraftingNodePresets.Presets.Find(PresetID);
		preset->Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		preset->Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
	}
	else
	{
		const FBIMPresetEditorNodeSharedPtr inst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
	}

	// Step 3: Get assets from key
	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByKey(rawMaterialKey);

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

bool ADynamicIconGenerator::SetIconMeshForModule(bool UseDependentPreset, const FBIMKey& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	// Step 1: Get params needed to make module icon
	FBIMKey rawMaterialKey;
	FString colorHexValue = TEXT("FFFFFF");

	Modumate::Units::FUnitValue 
		width = Modumate::Units::FUnitValue::WorldCentimeters(0), 
		length = Modumate::Units::FUnitValue::WorldCentimeters(0),
		depth = Modumate::Units::FUnitValue::WorldCentimeters(0),
		height = Modumate::Units::FUnitValue::WorldCentimeters(0);

	const FPresetManager &presetManager = GameState->Document.PresetManager;

	// Step 2: Should this icon be using its dependent presets, or use preset values from its children node?
	if (UseDependentPreset)
	{
		const FBIMPresetInstance* preset = presetManager.CraftingNodePresets.Presets.Find(PresetID);
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
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, width);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, length);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, depth);
		inst->WorkingPresetCopy.Properties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Height, height);
	}

	// Step 3: Get assets from key, and size from dimension
	FVector vSize = FVector::OneVector;

	const FArchitecturalMaterial* aMat = Gamemode->ObjectDatabase->GetArchitecturalMaterialByKey(rawMaterialKey);
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

bool ADynamicIconGenerator::SetIconMeshForLayerNodeID(const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget)
{
	FBIMAssemblySpec assembly;
	Controller->EditModelUserWidget->BIMDesigner->InstancePool.CreateAssemblyFromLayerNode(
		Controller->GetDocument()->PresetManager.CraftingNodePresets,
		*Gamemode->ObjectDatabase, NodeID, assembly);
	return SetIconMeshForWallAssembly(assembly, InRenderTarget);
}

bool ADynamicIconGenerator::SetIconMeshForLayerPreset(const FBIMKey& PresetID, UTextureRenderTarget2D* InRenderTarget)
{
	FBIMAssemblySpec assembly;
	EObjectType objType = EObjectType::OTWallSegment; // TODO: Get object type, default to wall for now
	EBIMResult result = Controller->GetDocument()->PresetManager.CraftingNodePresets.CreateAssemblyFromLayerPreset(*Gamemode->ObjectDatabase, PresetID, objType, assembly);
	return SetIconMeshForWallAssembly(assembly, InRenderTarget);
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

void ADynamicIconGenerator::SetSpringArmDistanceForCapture(AActor* ActorToCapture, float SizeScale, bool OnlyCollidingComponents)
{
	SavedSpringArmRelativeTransform = SpringArm->GetRelativeTransform();
	FVector captureForward = SceneCaptureComp->GetForwardVector();
	FVector actorBoxOrigin, actorBoxExtents;
	ActorToCapture->GetActorBounds(OnlyCollidingComponents, actorBoxOrigin, actorBoxExtents);
	float springArmCaptureDist = (actorBoxExtents.Size() * SizeScale) / FMath::Tan(FMath::DegreesToRadians(SceneCaptureComp->FOVAngle));
	SpringArm->SetWorldLocation(actorBoxOrigin - (springArmCaptureDist * captureForward));
}

void ADynamicIconGenerator::ResetSpringArm()
{
	SpringArm->SetRelativeTransform(SavedSpringArmRelativeTransform);
}

void ADynamicIconGenerator::ReleaseSavedRenderTarget()
{
	for (auto curRT : BIMKeyToRenderTarget)
	{
		curRT->ReleaseResource();
	}
	BIMKeyToRenderTarget.Empty();
}
