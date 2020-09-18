// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UnrealClasses/DynamicIconGenerator.h"
#include "Runtime/Engine/Classes/Components/RectLightComponent.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "ModumateCore/ModumateIconMeshStatics.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "ProceduralMeshComponent/Public/KismetProceduralMeshLibrary.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UI/EditModelUserWidget.h"
#include "UI/BIM/BIMDesigner.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateUnits.h"

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

	IconDynamicMeshActor = GetWorld()->SpawnActor<ADynamicMeshActor>(Gamemode->DynamicMeshActorClass.Get());
	IconDynamicMeshActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);

	IconCompoundMeshActor = GetWorld()->SpawnActor<ACompoundMeshActor>();
	IconCompoundMeshActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);

	DynSphereMaterial = IconSphereMesh->CreateDynamicMaterialInstance(0, IconSphereMaterial);
}

// Called every frame
void ADynamicIconGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool ADynamicIconGenerator::SetIconMeshForAssemblyByToolMode(bool UseAssemblyFromBIMDesigner, const FBIMKey& AsmKey, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	const FBIMAssemblySpec *assembly;
	EToolMode iconMode = mode;
	FModumateDocument *doc = &GameState->Document;
	if (UseAssemblyFromBIMDesigner)
	{
		assembly = &Controller->EditModelUserWidget->BIMDesigner->CraftingAssembly;
		iconMode = UModumateTypeStatics::ToolModeFromObjectType(assembly->ObjectType);
	}
	else
	{
		assembly = doc->PresetManager.GetAssemblyByKey(iconMode, AsmKey);
	}
	if (!assembly)
	{
		return false;
	}

	switch (iconMode)
	{
	case EToolMode::VE_WALL:
	case EToolMode::VE_FINISH:
		return SetIconMeshForWallAssembly(*assembly, iconMode, RenderTarget);
	case EToolMode::VE_FLOOR:
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_COUNTERTOP:
	case EToolMode::VE_CEILING:
		return SetIconMeshForFloorAssembly(*assembly, iconMode, RenderTarget);
	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
		return SetIconMeshForPortalAssembly(*assembly, iconMode, RenderTarget);
	case EToolMode::VE_STRUCTURELINE:
	case EToolMode::VE_TRIM:
		return SetIconMeshForTrimAssembly(*assembly, iconMode, RenderTarget);
	case EToolMode::VE_CABINET:
		return SetIconMeshForCabinetAssembly(*assembly, RenderTarget);
	case EToolMode::VE_PLACEOBJECT:
		return SetIconMeshForFFEAssembly(*assembly, RenderTarget);
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForBIMDesigner(const FBIMKey& PresetID, UTextureRenderTarget2D* RenderTarget, int32 NodeID)
{
	if (PresetID.IsNone())
	{
		return false;
	}
	const FPresetManager &presetManager = GameState->Document.PresetManager;
	const FBIMPreset* preset = presetManager.CraftingNodePresets.Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return false;
	}

	switch (preset->NodeScope)
	{
	case EBIMValueScope::RawMaterial:
		return SetIconMeshForRawMaterial(PresetID, RenderTarget);
	case EBIMValueScope::Color:
		return SetIconMeshForColor(PresetID, RenderTarget);
	case EBIMValueScope::Dimension:
		return SetIconMeshForDimension(NodeID, RenderTarget);
	case EBIMValueScope::Material:
		return SetIconMeshForMaterial(NodeID, RenderTarget);
	default:
		break;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForWallAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
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
	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconDynamicMeshLayersForCapture(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForFloorAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
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

	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconDynamicMeshLayersForCapture(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForPortalAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	IconCompoundMeshActor->MakeFromAssembly(Assembly, FVector::OneVector, false, true);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	float portalMinX = 0;// assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMinX).AsWorldCentimeters();
	float portalMaxX = 100; // assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMaxX).AsWorldCentimeters();
	float portalMinZ = 0; // assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMinZ).AsWorldCentimeters();
	float portalMaxZ = 180; // assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMaxZ).AsWorldCentimeters();
	float portalWidth = portalMaxX - portalMinX;
	float portalHeight = portalMaxZ - portalMinZ;

	FVector meshSize = (PortalIconScaleFactor / FVector(portalWidth, 0.f, portalHeight).Size()) * FVector::OneVector;
	FVector meshLocation = (FVector(portalWidth, 0.f, portalHeight)) * meshSize * -0.5f;

	IconCompoundMeshActor->SetActorRelativeLocation(meshLocation);
	IconCompoundMeshActor->SetActorRelativeScale3D(meshSize);

	// Set bound render to prevent mesh from being occluded by front mesh
	SetIconCompoundMeshActorForCapture(true);

	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconCompoundMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconCompoundMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconCompoundMeshActorForCapture(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForCabinetAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* RenderTarget)
{
	// Now that we have a cabinet assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a fake cabinet for icon generation the same way that FMOICabinetImpl does.

	// Get the toe kick dimensions from the assembly
	FVector2D toeKickDimensions;
	UModumateFunctionLibrary::GetCabinetToeKickDimensions(Assembly, toeKickDimensions);

	// Get the exterior finish material for the cabinet
	static const FName cabinetGeomMatName(TEXT("Cabinet_Exterior_Finish"));
	FArchitecturalMaterial materialData;

	// Get the cabinet dimensions
	Units::FUnitValue cabinetWidthUnit(CabinetDimension.X, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetHeightUnit(CabinetDimension.Z, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetDepthUnit(CabinetDimension.Y, Units::EUnitType::WorldInches);


	float cabinetWidth = cabinetWidthUnit.AsWorldCentimeters();
	float cabinetHeight = cabinetHeightUnit.AsWorldCentimeters();
	float cabinetDepth = cabinetDepthUnit.AsWorldCentimeters();

	// Make the cabinet base + box geometry
	TArray<FVector> cabinetBasePoints = {
		FVector::ZeroVector,
		FVector(cabinetWidth, 0.0f, 0.0f),
		FVector(cabinetWidth, cabinetDepth, 0.0f),
		FVector(0.0f, cabinetDepth, 0.0f),
	};
	FVector extrusionDelta = cabinetHeight * FVector::UpVector;

	IconDynamicMeshActor->SetupCabinetGeometry(cabinetBasePoints, extrusionDelta, materialData, false, false, toeKickDimensions, 2);
	IconCompoundMeshActor->MakeFromAssembly(Assembly, FVector::OneVector, false, false);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	// Scale IconDynamicMeshActor to fit
	FVector meshExtent = IconDynamicMeshActor->Mesh->Bounds.BoxExtent;
	FVector meshOrigin = IconDynamicMeshActor->Mesh->Bounds.Origin;
	FVector meshSize = ((CabinetIconScaleFactor / meshExtent.Size()) * FVector::OneVector);
	FVector meshLocation = FVector(0.f, 0.f, cabinetHeight * meshSize.Z * -0.5f);
	IconDynamicMeshActor->SetActorRelativeLocation(meshLocation);
	IconDynamicMeshActor->SetActorRelativeScale3D(meshSize);

	// Scale IconCompoundMeshActor to fit
	FTransform dynTransform = IconDynamicMeshActor->GetTransform();
	FVector newDynLocation = dynTransform.TransformPosition(IconCompoundMeshActor->GetTargetLocation());
	IconCompoundMeshActor->SetActorLocation(newDynLocation - FVector(meshSize.X * cabinetWidth, meshSize.Y * cabinetDepth, 0.f) * 0.5f);
	IconCompoundMeshActor->SetActorRelativeScale3D(meshSize);

	// Set bound render to prevent mesh from being occluded by front mesh
	SetIconCompoundMeshActorForCapture(true);
	IconDynamicMeshActor->Mesh->SetVisibility(true);
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, true);

	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconCompoundMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconCompoundMeshActor->SetActorScale3D(FVector::OneVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconCompoundMeshActorForCapture(false);
	IconDynamicMeshActor->Mesh->SetVisibility(false);
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForTrimAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	// Now that we have an assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a mesh for icon generation.
	FVector meshStartPos = FVector(TrimLength * 0.5f, 0.f, 0.f);
	FVector meshEndPos = FVector(TrimLength * -0.5f, 0.f, 0.f);
	FVector meshNormal = mode == EToolMode::VE_STRUCTURELINE ? FVector::LeftVector : FVector::RightVector;
	FVector meshUp = mode == EToolMode::VE_STRUCTURELINE ? FVector::DownVector : FVector::UpVector;

	FVector2D upperExtensions = FVector2D::ZeroVector;
	FVector2D outerExtensions = FVector2D::ZeroVector;

	FVector scaleVector;
	if (!Assembly.TryGetProperty(BIMPropertyNames::Scale, scaleVector))
	{
		scaleVector = FVector::OneVector;
	}
	IconDynamicMeshActor->SetupExtrudedPolyGeometry(Assembly, meshStartPos, meshEndPos,
		meshNormal, meshUp, upperExtensions, outerExtensions, scaleVector, true, false);

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

	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetComponentForIconCapture(IconDynamicMeshActor->Mesh, false);
	IconDynamicMeshActor->Mesh->SetVisibility(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForFFEAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Generate model
	////////////////////////////////////////////////////////////////////
	TArray<UStaticMesh*> meshes;
	UModumateIconMeshStatics::GetMeshesFromShoppingItem(Controller, Assembly.UniqueKey(), EToolMode::VE_PLACEOBJECT, meshes, false);
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
	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconStaticMesh->ResetRelativeTransform();
	SetComponentForIconCapture(IconStaticMesh, false);
	IconStaticMesh->SetVisibility(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForRawMaterial(const FBIMKey& MaterialKey, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Get material
	UMaterialInterface *mat;
	UModumateIconMeshStatics::GetEngineMaterialByKey(Controller, MaterialKey, mat);
	if (!mat)
	{
		return false;
	}
	IconSphereMesh->SetMaterial(0, mat);

	// Step 2: Setup mesh for material
	SetComponentForIconCapture(IconSphereMesh, true);
	IconSphereMesh->SetVisibility(true);
	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	SetComponentForIconCapture(IconSphereMesh, false);
	IconSphereMesh->SetVisibility(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForColor(const FBIMKey& ColorKey, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Get color
	FCustomColor customColor;
	if (UModumateIconMeshStatics::GetEngineCustomColorByKey(Controller, ColorKey, customColor))
	{
		IconSphereMesh->SetMaterial(0, DynSphereMaterial);
		DynSphereMaterial->SetVectorParameterValue(MaterialColorParamName, customColor.Color);

		// Step 2: Set color to material
		SetComponentForIconCapture(IconSphereMesh, true);
		IconSphereMesh->SetVisibility(true);
		SceneCaptureComp->TextureTarget = RenderTarget;
		SceneCaptureComp->CaptureScene();

		// Step 3: Cleanup
		SetComponentForIconCapture(IconSphereMesh, false);
		IconSphereMesh->SetVisibility(false);
		return true;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForDimension(int32 NodeID, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Get params
	FString width, length, depth, height, thickness;
	GetDimensionFromNode(NodeID, width, length, depth, height, thickness);
	FVector vSize = FVector::OneVector;
	if (!width.IsEmpty())
	{
		vSize.Y = UModumateDimensionStatics::StringToFormattedDimension(width).Centimeters;
	}
	if (!length.IsEmpty())
	{
		vSize.X = UModumateDimensionStatics::StringToFormattedDimension(length).Centimeters;
	}
	if (!depth.IsEmpty())
	{
		vSize.Y = UModumateDimensionStatics::StringToFormattedDimension(depth).Centimeters;
	}
	if (!height.IsEmpty())
	{
		vSize.Z = UModumateDimensionStatics::StringToFormattedDimension(height).Centimeters;
	}

	FVector nSize = vSize.GetSafeNormal() * 0.5f;
	IconCubeMesh->SetWorldScale3D(nSize);

	// Step 2: Set for capture
	SetComponentForIconCapture(IconCubeMesh, true);
	IconCubeMesh->SetVisibility(true);
	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconCubeMesh->ResetRelativeTransform();
	SetComponentForIconCapture(IconCubeMesh, false);
	IconCubeMesh->SetVisibility(false);

	return true;
}

bool ADynamicIconGenerator::SetIconMeshForMaterial(int32 NodeID, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Get params
	UMaterialInterface *mat = IconSphereMaterial;
	FCustomColor customColor;
	const FPresetManager &presetManager = GameState->Document.PresetManager;
	const FBIMCraftingTreeNodeSharedPtr inst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);

	TArray<FBIMCraftingTreeNodeSharedPtr> childrenNodes;
	inst->GatherAllChildNodes(childrenNodes);

	for (const auto& child : childrenNodes)
	{
		const FBIMPreset* preset = presetManager.CraftingNodePresets.Presets.Find(child->PresetID);
		if (preset->NodeScope == EBIMValueScope::RawMaterial)
		{
			UModumateIconMeshStatics::GetEngineMaterialByKey(Controller, child->PresetID, mat);
		}
		if (preset->NodeScope == EBIMValueScope::Color)
		{
			UModumateIconMeshStatics::GetEngineCustomColorByKey(Controller, child->PresetID, customColor);
		}
	}
	// Step 2: Set color to material
	if (customColor.bValid)
	{
		IconSphereMesh->SetMaterial(0, mat);
		auto* dynMat = IconSphereMesh->CreateDynamicMaterialInstance(0, mat);
		dynMat->SetVectorParameterValue(MaterialColorParamName, customColor.Color);

		SetComponentForIconCapture(IconSphereMesh, true);
		IconSphereMesh->SetVisibility(true);
		SceneCaptureComp->TextureTarget = RenderTarget;
		SceneCaptureComp->CaptureScene();

		// Step 3: Cleanup
		SetComponentForIconCapture(IconSphereMesh, false);
		IconSphereMesh->SetVisibility(false);
		return true;
	}
	return false;
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

void ADynamicIconGenerator::GetDimensionFromNode(int32 NodeID, FString &OutWidth, FString &OutLength, FString &OutDepth, FString &OutHeight, FString &OutThickness)
{
	const FBIMCraftingTreeNodeSharedPtr inst = Controller->EditModelUserWidget->BIMDesigner->InstancePool.InstanceFromID(NodeID);
	inst->InstanceProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, OutWidth);
	inst->InstanceProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, OutLength);
	inst->InstanceProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, OutDepth);
	inst->InstanceProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Height, OutHeight);
	inst->InstanceProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, OutThickness);
}
