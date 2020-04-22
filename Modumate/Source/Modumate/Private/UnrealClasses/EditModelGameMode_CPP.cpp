// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelGameMode_CPP.h"

#include "EditModelPlayerState_CPP.h"
#include "HAL/FileManager.h"
#include "EditModelToolInterface.h"
#include "Kismet/GameplayStatics.h"
#include "LineActor3D_CPP.h"
#include "MOIGroupActor_CPP.h"
#include "ModumateObjectDatabase.h"
#include "PortalFrameActor_CPP.h"
#include "UObject/ConstructorHelpers.h"

using namespace Modumate;

AEditModelGameMode_CPP::AEditModelGameMode_CPP()
{
	PlayerStateClass = AEditModelPlayerState_CPP::StaticClass();
	DynamicMeshActorClass = ADynamicMeshActor::StaticClass();

	ObjectDatabase = new Modumate::ModumateObjectDatabase();

	ObjectDatabase->Init();
}

AEditModelGameMode_CPP::~AEditModelGameMode_CPP()
{
	ObjectDatabase->Shutdown();
	delete ObjectDatabase;
}

void AEditModelGameMode_CPP::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	double databaseLoadTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(databaseLoadTime);

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

	}
	databaseLoadTime *= 1000.0;
	UE_LOG(LogPerformance, Log, TEXT("Object database loaded in %d ms"), int(databaseLoadTime + 0.5));

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

