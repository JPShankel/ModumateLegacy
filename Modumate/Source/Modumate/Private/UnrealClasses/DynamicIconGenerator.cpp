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
}

// Called when the game starts or when spawned
void ADynamicIconGenerator::BeginPlay()
{
	Super::BeginPlay();

	AEditModelGameMode_CPP* gamemode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();

	IconDynamicMeshActor = GetWorld()->SpawnActor<ADynamicMeshActor>(gamemode->DynamicMeshActorClass.Get());
	IconDynamicMeshActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);

	IconCompoundMeshActor = GetWorld()->SpawnActor<ACompoundMeshActor>();
	IconCompoundMeshActor->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
}

// Called every frame
void ADynamicIconGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool ADynamicIconGenerator::SetIconMeshForAssemblyByToolMode(const FName &AsmKey, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	switch (mode)
	{
	case EToolMode::VE_WALL:
	case EToolMode::VE_FINISH:
		SetIconMeshForWallAssembly(AsmKey, mode, RenderTarget);
		return true;
	case EToolMode::VE_FLOOR:
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_COUNTERTOP:
		SetIconMeshForFloorAssembly(AsmKey, mode, RenderTarget);
		return true;
	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
		SetIconMeshForPortalAssembly(AsmKey, mode, RenderTarget);
		return true;
	case EToolMode::VE_STRUCTURELINE:
	case EToolMode::VE_TRIM:
		SetIconMeshForTrimAssembly(AsmKey, mode, RenderTarget);
		return true;
	case EToolMode::VE_CABINET:
		SetIconMeshForCabinetAssembly(AsmKey, RenderTarget);
		return true;
	case EToolMode::VE_PLACEOBJECT:
		SetIconMeshForFFEAssembly(AsmKey, RenderTarget);
		return true;
	}
	return false;
}

bool ADynamicIconGenerator::SetIconMeshForWallAssembly(const FName &AsmKey, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Generate model
	////////////////////////////////////////////////////////////////////
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

	const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByKey(mode, AsmKey);
	if (!assembly)
	{
		return false;
	}
	////////////////////////////////////////////////////////////////////
	FVector p1 = FVector(WallLength * 0.5f, 0.f, 0.f) + Root->GetComponentLocation();
	FVector p2 = FVector(WallLength * -0.5f, 0.f, 0.f) + Root->GetComponentLocation();
	FVector p3 = FVector(WallLength * -0.5f, 0.f, WallHeight) + Root->GetComponentLocation();
	FVector p4 = FVector(WallLength * 0.5f, 0.f, WallHeight) + Root->GetComponentLocation();
	TArray<FVector> planePoints = { p1, p2, p3, p4};

	IconDynamicMeshActor->CreateBasicLayerDefs(planePoints, FVector::ZeroVector, *assembly, 0.5f, FVector::ZeroVector);
	IconDynamicMeshActor->Assembly = *assembly;
	IconDynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);

	// Slice each layer, assuming each layer is successfully made
	for (int32 i = 0; i < assembly->CachedAssembly.Layers.Num(); ++i)
	{
		if (IconDynamicMeshActor->ProceduralSubLayers[i])
		{
			FVector sliceLocation, sliceNormal;
			GetWallSliceLocationNormal(i, assembly->CachedAssembly.Layers.Num(), p2, p1, WallHeight, sliceLocation, sliceNormal);
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
	FVector meshLocation = (meshOrigin - GetActorLocation()) * meshExtent * -1.f;
	IconDynamicMeshActor->SetActorRelativeLocation(FVector(meshLocation.X, meshLocation.Y, 0.f));
	IconDynamicMeshActor->SetActorScale3D(meshSize);
	SceneCaptureComp->TextureTarget = RenderTarget;
	SceneCaptureComp->CaptureScene();

	// Step 3: Cleanup
	IconDynamicMeshActor->SetActorRelativeLocation(FVector::ZeroVector);
	IconDynamicMeshActor->SetActorScale3D(FVector::OneVector);
	SetIconDynamicMeshLayersForCapture(false);
	return true;
}

bool ADynamicIconGenerator::SetIconMeshForFloorAssembly(const FName &AsmKey, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Generate model
	////////////////////////////////////////////////////////////////////
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

	const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByKey(mode, AsmKey);
	if (!assembly)
	{
		return false;
	}
	////////////////////////////////////////////////////////////////////
	FVector p1 = FVector(FloorLength * -0.5f, FloorDepth * 0.5f, 0.f) + Root->GetComponentLocation();
	FVector p2 = FVector(FloorLength * 0.5f, FloorDepth * 0.5f, 0.f) + Root->GetComponentLocation();
	FVector p3 = FVector(FloorLength * 0.5f, FloorDepth * -0.5f, 0.f) + Root->GetComponentLocation();
	FVector p4 = FVector(FloorLength * -0.5f, FloorDepth * -0.5f, 0.f) + Root->GetComponentLocation();
	TArray<FVector> planePoints = { p1, p2, p3, p4 };

	IconDynamicMeshActor->CreateBasicLayerDefs(planePoints, FVector::ZeroVector, *assembly, 1.f, FVector::ZeroVector);
	IconDynamicMeshActor->Assembly = *assembly;
	IconDynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);

	// Slice each layer, assuming each layer is successfully made
	for (int32 i = 0; i < assembly->CachedAssembly.Layers.Num(); ++i)
	{
		if (IconDynamicMeshActor->ProceduralSubLayers[i])
		{
			FVector sliceLocation, sliceNormal;
			bool bSliced;
			float layerRatio = 1.f / assembly->CachedAssembly.Layers.Num();
			FVector sliceStart = p4 + layerRatio * (p3 - p4);
			FVector sliceEnd = p2 + layerRatio * (p1 - p2);

			GetFloorSliceLocationNormal(i, assembly->CachedAssembly.Layers.Num(), sliceStart, sliceEnd, 0.f, sliceLocation, sliceNormal, bSliced);
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
	FVector meshLocation = (meshOrigin - GetActorLocation())* meshSize * -1.f;
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

bool ADynamicIconGenerator::SetIconMeshForPortalAssembly(const FName &AsmKey, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Generate model
	////////////////////////////////////////////////////////////////////
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

	const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByKey(mode, AsmKey);
	if (!assembly)
	{
		return false;
	}
	////////////////////////////////////////////////////////////////////
	IconCompoundMeshActor->MakeFromAssembly(*assembly, FVector::OneVector, false, true);

	// Step 2: Calculate and adjust model to fit inside the view of SceneCaptureComp
	float portalMinX = assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMinX).AsWorldCentimeters();
	float portalMaxX = assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMaxX).AsWorldCentimeters();
	float portalMinZ = assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMinZ).AsWorldCentimeters();
	float portalMaxZ = assembly->CachedAssembly.PortalConfiguration.CachedDimensions.FindRef(FPortalConfiguration::RefPlaneNameMaxZ).AsWorldCentimeters();
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

bool ADynamicIconGenerator::SetIconMeshForCabinetAssembly(const FName &AsmKey, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Generate model
////////////////////////////////////////////////////////////////////
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

	const FBIMAssemblySpec *assemblyPtr = doc->PresetManager.GetAssemblyByKey(EToolMode::VE_CABINET,AsmKey);
	if (!assemblyPtr)
	{
		return false;
	}

	// need to make local changes, so copy...
	FModumateObjectAssembly assembly = assemblyPtr->CachedAssembly;

	////////////////////////////////////////////////////////////////////
	// Now that we have a cabinet assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a fake cabinet for icon generation the same way that FMOICabinetImpl does.

	// Get the toe kick dimensions from the assembly
	FVector2D toeKickDimensions;
	UModumateFunctionLibrary::GetCabinetToeKickDimensions(*assemblyPtr, toeKickDimensions);

	// Get the exterior finish material for the cabinet
	static const FName cabinetGeomMatName(TEXT("Cabinet_Exterior_Finish"));
	FArchitecturalMaterial materialData;
	if (assembly.PortalConfiguration.IsValid())
	{
		materialData = assembly.PortalConfiguration.MaterialsPerChannel.FindRef(cabinetGeomMatName);
	}

	// Get the cabinet dimensions
	Units::FUnitValue cabinetWidthUnit(CabinetDimension.X, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetHeightUnit(CabinetDimension.Z, Units::EUnitType::WorldInches);
	Units::FUnitValue cabinetDepthUnit(CabinetDimension.Y, Units::EUnitType::WorldInches);

	if (assembly.PortalConfiguration.IsValid())
	{
		cabinetWidthUnit = assembly.PortalConfiguration.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxX].FixedValue;
		cabinetHeightUnit = assembly.PortalConfiguration.ReferencePlanes[FPortalConfiguration::RefPlaneNameMaxZ].FixedValue;
	}

	float cabinetWidth = cabinetWidthUnit.AsWorldCentimeters();
	float cabinetHeight = cabinetHeightUnit.AsWorldCentimeters();
	float cabinetDepth = cabinetDepthUnit.AsWorldCentimeters();

	// Make the cabinet base + box geometry
	TArray<FVector> cabinetCPs = {
		FVector::ZeroVector,
		FVector(cabinetWidth, 0.0f, 0.0f),
		FVector(cabinetWidth, cabinetDepth, 0.0f),
		FVector(0.0f, cabinetDepth, 0.0f),
	};
	IconDynamicMeshActor->SetupCabinetGeometry(cabinetCPs, cabinetHeight, materialData, toeKickDimensions, 2);

	// Now make the cabinet portal geometry
	if (assembly.PortalConfiguration.IsValid())
	{
		// Update the reference planes so the portal 9-slicing is correct
		TMap<FName, FPortalReferencePlane> &refPlanes = assembly.PortalConfiguration.ReferencePlanes;
		refPlanes[FPortalConfiguration::RefPlaneNameMinX].FixedValue = Units::FUnitValue::WorldCentimeters(0.0f);
		refPlanes[FPortalConfiguration::RefPlaneNameMaxX].FixedValue = Units::FUnitValue::WorldCentimeters(cabinetWidth);
		refPlanes[FPortalConfiguration::RefPlaneNameMinZ].FixedValue = Units::FUnitValue::WorldCentimeters(0.0f);
		refPlanes[FPortalConfiguration::RefPlaneNameMaxZ].FixedValue = Units::FUnitValue::WorldCentimeters(cabinetHeight - toeKickDimensions.Y);
		assembly.PortalConfiguration.CacheRefPlaneValues();
	}

	IconCompoundMeshActor->MakeFromAssembly(*assemblyPtr, FVector::OneVector, false, false);

	if (assembly.PortalConfiguration.IsValid())
	{
		// Now position the portal where it's supposed to go
		FVector portalOrigin = cabinetCPs[3] + (toeKickDimensions.Y * FVector::UpVector);
		FQuat portalRot = FRotationMatrix::MakeFromYZ(FVector(0.0f, 1.0f, 0.0f), FVector::UpVector).ToQuat();

		IconCompoundMeshActor->SetActorLocationAndRotation(portalOrigin, portalRot);
	}

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

bool ADynamicIconGenerator::SetIconMeshForTrimAssembly(const FName &AsmKey, EToolMode mode, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Generate model
	////////////////////////////////////////////////////////////////////
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

	const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByKey(mode, AsmKey);
	if (!assembly)
	{
		return false;
	}
	////////////////////////////////////////////////////////////////////
	// Now that we have an assembly, a DynamicMeshActor, and CompoundMeshActor,
	// we can make a mesh for icon generation.
	FVector meshStartPos = FVector(TrimLength * 0.5f, 0.f, 0.f);
	FVector meshEndPos = FVector(TrimLength * -0.5f, 0.f, 0.f);
	FVector meshNormal = mode == EToolMode::VE_STRUCTURELINE ? FVector::LeftVector : FVector::RightVector;
	FVector meshUp = mode == EToolMode::VE_STRUCTURELINE ? FVector::DownVector : FVector::UpVector;

	FVector2D upperExtensions = FVector2D::ZeroVector;
	FVector2D outerExtensions = FVector2D::ZeroVector;

	FVector scaleVector;
	if (!assembly->CachedAssembly.TryGetProperty(BIM::Parameters::Scale, scaleVector))
	{
		scaleVector = FVector::OneVector;
	}
	IconDynamicMeshActor->SetupExtrudedPolyGeometry(*assembly, meshStartPos, meshEndPos,
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

bool ADynamicIconGenerator::SetIconMeshForFFEAssembly(const FName &AsmKey, UTextureRenderTarget2D* RenderTarget)
{
	// Step 1: Generate model
	////////////////////////////////////////////////////////////////////
	TArray<UStaticMesh*> meshes;
	AEditModelPlayerController_CPP *controller = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
	UModumateIconMeshStatics::GetMeshesFromShoppingItem(controller, AsmKey, EToolMode::VE_PLACEOBJECT, meshes, false);
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
