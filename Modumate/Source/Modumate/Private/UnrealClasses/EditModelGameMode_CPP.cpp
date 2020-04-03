// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelGameMode_CPP.h"

#include "EditModelPlayerState_CPP.h"
#include "HAL/FileManager.h"
#include "IModumateEditorTool.h"
#include "Kismet/GameplayStatics.h"
#include "LineActor3D_CPP.h"
#include "MOIGroupActor_CPP.h"
#include "ModumateObjectDatabase.h"
#include "PortalFrameActor_CPP.h"
#include "UObject/ConstructorHelpers.h"

using namespace Modumate;


UMaterialInterface *AEditModelGameMode_CPP::VolumeHoverMaterial;
UMaterialInterface *AEditModelGameMode_CPP::RoomHandleMaterial;
UMaterialInterface *AEditModelGameMode_CPP::RoomHandleMaterialNoDepth;

UStaticMesh *AEditModelGameMode_CPP::AnchorMesh;
UStaticMesh *AEditModelGameMode_CPP::PointAdjusterMesh;
UStaticMesh *AEditModelGameMode_CPP::FaceAdjusterMesh;
UStaticMesh *AEditModelGameMode_CPP::RotateHandleMesh;
UStaticMesh *AEditModelGameMode_CPP::InvertHandleMesh;

UMaterialInterface *AEditModelGameMode_CPP::GreenMaterial;
UMaterialInterface *AEditModelGameMode_CPP::HideMaterial;
UMaterialInterface *AEditModelGameMode_CPP::LinePlaneSegmentMaterial;

UClass *AEditModelGameMode_CPP::LineClass;
UClass *AEditModelGameMode_CPP::PortalFrameActorClass;
UClass *AEditModelGameMode_CPP::MOIGroupActorClass;


AEditModelGameMode_CPP::AEditModelGameMode_CPP()
{
	PlayerStateClass = AEditModelPlayerState_CPP::StaticClass();
	DynamicMeshActorClass = ADynamicMeshActor::StaticClass();

	ObjectDatabase = new Modumate::ModumateObjectDatabase();


	// Initialize the database here since we also use ConstructorHelpers::FObjectFinders there.
	ObjectDatabase->Init();

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VolumeHoverMaterialAsset(TEXT("Material'/Game/Materials/MaterialAffordance/M_VolumeHover.M_VolumeHover'"));
	VolumeHoverMaterial = VolumeHoverMaterialAsset.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoomHandleMaterialAsset(TEXT("Material'/Game/Materials/MaterialAffordance/M_RoomInfoHandle.M_RoomInfoHandle'"));
	RoomHandleMaterial = RoomHandleMaterialAsset.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoomHandleNoDepthMaterialAsset(TEXT("Material'/Game/Materials/MaterialAffordance/M_RoomInfoHandle_NoDepth.M_RoomInfoHandle_NoDepth'"));
	RoomHandleMaterialNoDepth = RoomHandleNoDepthMaterialAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Anchor(TEXT("StaticMesh'/Game/Meshes/BasicShapes/Shape_QuadPyramid.Shape_QuadPyramid'"));
	AnchorMesh = Anchor.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FaceAdjusterAsset(TEXT("StaticMesh'/Game/Meshes/BasicShapes/SM_Face_Adjuster.SM_Face_Adjuster'"));
	FaceAdjusterMesh = FaceAdjusterAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PointAdjusterAsset(TEXT("StaticMesh'/Game/Meshes/BasicShapes/SM_Point_Adjuster.SM_Point_Adjuster'"));
	PointAdjusterMesh = PointAdjusterAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RotateHandleAsset(TEXT("StaticMesh'/Game/Meshes/BasicShapes/SM_Rotate_Handle.SM_Rotate_Handle'"));
	RotateHandleMesh = RotateHandleAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> InvertHandleAsset(TEXT("StaticMesh'/Game/Meshes/BasicShapes/SM_InvertHandle.SM_InvertHandle'"));
	InvertHandleMesh = InvertHandleAsset.Object;

	static ConstructorHelpers::FClassFinder<ALineActor3D_CPP> LineAsset(TEXT("/Game/Blueprints/BPComponents/LineActor3D_BP"));
	LineClass = LineAsset.Class;

	static ConstructorHelpers::FClassFinder<APortalFrameActor_CPP> PortalFrameActorAsset(TEXT("/Game/Blueprints/EditModel/PortalFrameActor_BP"));
	PortalFrameActorClass = PortalFrameActorAsset.Class;

	static ConstructorHelpers::FClassFinder<AMOIGroupActor_CPP> MOIGroupActorAsset(TEXT("/Game/Blueprints/EditModel/MOIGroupActor_BP"));
	MOIGroupActorClass = MOIGroupActorAsset.Class;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HideMatRef(TEXT("Material'/Game/Materials/M_HideMOI_Master.M_HideMOI_Master'"));
	HideMaterial = HideMatRef.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LinePlaneSegmentMaterialRef(TEXT("Material'/Game/Materials/MaterialAffordance/M_PendingSegmentPlane.M_PendingSegmentPlane'"));
	LinePlaneSegmentMaterial = LinePlaneSegmentMaterialRef.Object;
}

AEditModelGameMode_CPP::~AEditModelGameMode_CPP()
{
	ObjectDatabase->Shutdown();
	delete ObjectDatabase;
}

void AEditModelGameMode_CPP::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ObjectDatabase->ReadAMaterialData(MaterialTable);
	ObjectDatabase->ReadColorData(ColorTable);
	ObjectDatabase->ReadMeshData(MeshTable);
	ObjectDatabase->ReadPortalPartData(PortalPartsTable);
	ObjectDatabase->ReadLightConfigData(LightConfigTable);
	ObjectDatabase->ReadRoomConfigurations(RoomConfigurationTable);

	ObjectDatabase->ReadCraftingSubcategoryData(CraftingSubcategoryTable);

	ObjectDatabase->ReadCraftingPatternOptionSet(PatternOptionSetDataTable);
	ObjectDatabase->ReadCraftingPortalPartOptionSet(PortalPartOptionSetDataTable);

	ObjectDatabase->ReadCraftingMaterialAndColorOptionSet(MaterialsAndColorsOptionSetDataTable);
	ObjectDatabase->ReadCraftingDimensionalOptionSet(DimensionalOptionSetDataTable);
	ObjectDatabase->ReadCraftingLayerThicknessOptionSet(LayerThicknessOptionSetTable);
	ObjectDatabase->ReadPortalConfigurationData(PortalConfigurationTable);

	ObjectDatabase->ReadFFEPartData(FFEPartTable);
	ObjectDatabase->ReadFFEAssemblyData(FFEAssemblyTable);

	ObjectDatabase->ReadCraftingProfileOptionSet(ProfileMeshDataTable);

	ObjectDatabase->ReadMarketplace(GetWorld());

	const FString projectPathKey(TEXT("LoadFile"));
	if (UGameplayStatics::HasOption(Options, projectPathKey))
	{
		FString projectPathOption = UGameplayStatics::ParseOption(Options, projectPathKey);
		if (IFileManager::Get().FileExists(*projectPathOption))
		{
			PendingProjectPath = projectPathOption;
		}
		else if (!FParse::QuotedString(*projectPathOption, PendingProjectPath))
		{
			PendingProjectPath.Empty();
		}

		if (IFileManager::Get().FileExists(*PendingProjectPath))
		{
			UE_LOG(LogCallTrace, Log, TEXT("Will open project: \"%s\""), *PendingProjectPath);
		}
	}
}

