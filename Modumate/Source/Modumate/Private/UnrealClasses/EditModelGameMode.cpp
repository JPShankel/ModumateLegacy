// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameMode.h"

#include "Database/ModumateObjectDatabase.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/MOIGroupActor.h"
#include "UnrealClasses/PortalFrameActor.h"
#include "UObject/ConstructorHelpers.h"

using namespace Modumate;

AEditModelGameMode::AEditModelGameMode()
	: ObjectDatabase(nullptr)
{
	PlayerStateClass = AEditModelPlayerState::StaticClass();
	DynamicMeshActorClass = ADynamicMeshActor::StaticClass();
}

void AEditModelGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ObjectDatabase)
	{
		ObjectDatabase->Shutdown();
		delete ObjectDatabase;
		ObjectDatabase = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AEditModelGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ObjectDatabase = new FModumateDatabase();
	ObjectDatabase->Init();

	double databaseLoadTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(databaseLoadTime);
		ObjectDatabase->ReadPresetData();
	}
	databaseLoadTime *= 1000.0;
	UE_LOG(LogPerformance, Log, TEXT("Object database loaded in %d ms"), int(databaseLoadTime + 0.5));
}

void AEditModelGameMode::InitGameState()
{
	Super::InitGameState();

	auto* gameState = Cast<AEditModelGameState>(GameState);
	if (ensure(gameState))
	{
		gameState->InitDocument();
	}
}
