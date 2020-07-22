// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameMode_CPP.h"

#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "HAL/FileManager.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/MOIGroupActor_CPP.h"
#include "Database/ModumateObjectDatabase.h"
#include "UnrealClasses/PortalFrameActor_CPP.h"
#include "UObject/ConstructorHelpers.h"

using namespace Modumate;

AEditModelGameMode_CPP::AEditModelGameMode_CPP()
{
	PlayerStateClass = AEditModelPlayerState_CPP::StaticClass();
	DynamicMeshActorClass = ADynamicMeshActor::StaticClass();

	ObjectDatabase = new FModumateDatabase();

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

		ObjectDatabase->ReadColorData(ColorTable);
		ObjectDatabase->ReadMeshData(MeshTable);
		ObjectDatabase->ReadPortalPartData(PortalPartsTable);
		ObjectDatabase->ReadLightConfigData(LightConfigTable);
		ObjectDatabase->ReadRoomConfigurations(RoomConfigurationTable);

		ObjectDatabase->ReadCraftingPatternOptionSet(PatternOptionSetDataTable);
		ObjectDatabase->ReadCraftingProfileOptionSet(ProfileMeshDataTable);

		ObjectDatabase->ReadCraftingPortalPartOptionSet(PortalPartOptionSetDataTable);
		ObjectDatabase->ReadPortalConfigurationData(PortalConfigurationTable);

		ObjectDatabase->ReadFFEPartData(FFEPartTable);
		ObjectDatabase->ReadFFEAssemblyData(FFEAssemblyTable);

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

