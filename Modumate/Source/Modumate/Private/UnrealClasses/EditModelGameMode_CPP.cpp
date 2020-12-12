// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameMode_CPP.h"

#include "Database/ModumateObjectDatabase.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/MOIGroupActor_CPP.h"
#include "UnrealClasses/PortalFrameActor_CPP.h"
#include "UObject/ConstructorHelpers.h"

using namespace Modumate;

AEditModelGameMode_CPP::AEditModelGameMode_CPP()
	: ObjectDatabase(nullptr)
{
	PlayerStateClass = AEditModelPlayerState_CPP::StaticClass();
	DynamicMeshActorClass = ADynamicMeshActor::StaticClass();
}

void AEditModelGameMode_CPP::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ObjectDatabase)
	{
		ObjectDatabase->Shutdown();
		delete ObjectDatabase;
		ObjectDatabase = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AEditModelGameMode_CPP::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ObjectDatabase = new FModumateDatabase();
	ObjectDatabase->Init();

	double databaseLoadTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(databaseLoadTime);

		ObjectDatabase->ReadRoomConfigurations(RoomConfigurationTable);

		ObjectDatabase->ReadPresetData();
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

void AEditModelGameMode_CPP::InitGameState()
{
	Super::InitGameState();

	auto* gameState = Cast<AEditModelGameState_CPP>(GameState);
	if (ensure(gameState))
	{
		gameState->InitDocument();
	}
}
