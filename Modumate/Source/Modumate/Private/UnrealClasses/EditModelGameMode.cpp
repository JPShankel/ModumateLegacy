// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameMode.h"

#include "HttpManager.h"
#include "Kismet/GameplayStatics.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateCloudConnection.h"
#include "Online/ProjectConnection.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"

#define LOCTEXT_NAMESPACE "ModumateOnline"

const FString AEditModelGameMode::ProjectIDArg(TEXT("-ModProjectID="));
const FString AEditModelGameMode::APIKeyArg(TEXT("-ModApiKey="));
const FString AEditModelGameMode::EncryptionTokenKey(TEXT("EncryptionToken"));

AEditModelGameMode::AEditModelGameMode()
	: Super()
{
	PlayerStateClass = AEditModelPlayerState::StaticClass();
	DynamicMeshActorClass = ADynamicMeshActor::StaticClass();
}

void AEditModelGameMode::InitGameState()
{
	Super::InitGameState();

	auto* gameState = Cast<AEditModelGameState>(GameState);

	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;

	if (!ensure(gameState && accountManager.IsValid() && cloudConnection.IsValid()))
	{
		return;
	}

	FString localUserID;
	int32 localUserIdx = UModumateDocument::MaxUserIdx;

#if !UE_SERVER
	if (accountManager.IsValid() && cloudConnection.IsValid() && cloudConnection->IsLoggedIn())
	{
		localUserID = accountManager->GetUserInfo().ID;
	}
	localUserIdx = 0;
#endif

	gameState->InitDocument(localUserID, localUserIdx);

#if UE_SERVER
	FString inXApiKey;
	if (FParse::Value(FCommandLine::Get(), *AEditModelGameMode::ProjectIDArg, CurProjectID) && !CurProjectID.IsEmpty() &&
		FParse::Value(FCommandLine::Get(), *AEditModelGameMode::APIKeyArg, inXApiKey) && !inXApiKey.IsEmpty())
	{
		cloudConnection->SetXApiKey(inXApiKey);

		FString redactedKey = FString::Printf(TEXT("%c****%c"), inXApiKey[0], inXApiKey[inXApiKey.Len() - 1]);
		UE_LOG(LogGameMode, Log, TEXT("Starting EditModelGameMode server with Project ID: %s, API Key: %s"), *CurProjectID, *redactedKey);
	}
	else
	{
		UE_LOG(LogGameMode, Fatal, TEXT("Can't start an EditModelGameMode server without %s and %s arguments!"), *AEditModelGameMode::ProjectIDArg, *AEditModelGameMode::APIKeyArg);
		FPlatformMisc::RequestExit(false);
		return;
	}

	gameState->DownloadDocument();
	gameInstance->GetTimerManager().SetTimer(ServerStatusTimer, this, &AEditModelGameMode::OnServerStatusTimer, ServerStatusTimerRate, true);
#endif
}

void AEditModelGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// Non-dedicated servers can just use default behavior; no one should be logging into them.
#if UE_SERVER
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (!cloudConnection.IsValid())
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid ModumateCloudConnection!"));
		ErrorMessage = FString(TEXT("Server is not ready; please try again later."));
		return;
	}

	FString encryptionToken = UGameplayStatics::ParseOption(Options, AEditModelGameMode::EncryptionTokenKey);

	// Now, get the encryption key for the encryption token, which should already be cached on the CloudConnection,
	// assuming the user already successfully shook hands via our encrypted packet handler.
	FString userID, projectID, encryptionKey;
	if (!FModumateCloudConnection::ParseEncryptionToken(encryptionToken, userID, projectID) ||
		!cloudConnection->GetCachedEncryptionKey(userID, projectID, encryptionKey))
	{
		UE_LOG(LogGameMode, Error, TEXT("Couldn't get encryption key for token: %s"), *encryptionToken);
		ErrorMessage = FString(TEXT("Login failed; check credentials and try again later."));
		return;
	}

	if (projectID != CurProjectID)
	{
		UE_LOG(LogGameMode, Error, TEXT("Input project ID \"%s\" doesn't match current project ID: %s"), *projectID, *CurProjectID);
		ErrorMessage = FString(TEXT("Invalid session ID; make sure you've selected the correct server and try again."));
		return;
	}

	if (PlayersByUserID.Contains(userID))
	{
		UE_LOG(LogGameMode, Error, TEXT("UserID %s is already logged in!"), *userID);
		ErrorMessage = FString(TEXT("Duplicate login; please try again later."));
	}
	else
	{
		UE_LOG(LogGameMode, Log, TEXT("UserID %s login approved!"), *userID);
		PlayersByUserID.Add(userID, INDEX_NONE);
	}
#endif
}

APlayerController* AEditModelGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	APlayerController* newController = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);

#if UE_SERVER
	FString userID, projectID;
	FString encryptionToken = UGameplayStatics::ParseOption(Options, AEditModelGameMode::EncryptionTokenKey);
	APlayerState* playerState = newController ? newController->PlayerState : nullptr;
	int32 playerID = playerState ? playerState->GetPlayerId() : INDEX_NONE;

	if ((playerID == INDEX_NONE) || UsersByPlayerID.Contains(playerID) ||
		!FModumateCloudConnection::ParseEncryptionToken(encryptionToken, userID, projectID) ||
		!PlayersByUserID.Contains(userID))
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid/duplicate Player ID: %d and User ID: %s"), playerID, *userID);
		ErrorMessage = FString(TEXT("Log in failure"));
		return newController;
	}

	UsersByPlayerID.Add(playerID, userID);
	PlayersByUserID[userID] = playerID;
#endif

	return newController;
}

void AEditModelGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

#if UE_SERVER
	// TODO: this should probably send the deltas that have been applied since the version of the document that the joining player has downloaded,
	// rather than just the last time we saved - since we could have auto-saved just as the player joined.
	auto* playerState = NewPlayer ? Cast<AEditModelPlayerState>(NewPlayer->PlayerState) : nullptr;
	int32 playerID = playerState ? playerState->GetPlayerId() : INDEX_NONE;
	FString userID = UsersByPlayerID.FindRef(playerID);
	auto* gameState = Cast<AEditModelGameState>(GameState);
	if (ensure(playerState && gameState && !userID.IsEmpty()))
	{
		TArray<FDeltasRecord> deltasSinceSave;
		if (gameState->GetDeltaRecordsSinceSave(deltasSinceSave))
		{
			int32 numDeltasSinceSave = deltasSinceSave.Num();
			if (numDeltasSinceSave > 0)
			{
				UE_LOG(LogGameMode, Log, TEXT("Sent %d DeltasRecords to User ID %s that were made since the document was last saved."), numDeltasSinceSave, *userID);
				playerState->SendInitialDeltas(deltasSinceSave);
			}
		}
		else
		{
			UE_LOG(LogGameMode, Error, TEXT("Failed to get unsaved DeltasRecords for User ID %s!"), *userID);
			// TODO: if this failed, then we should either disconnect the user, or tell them to re-download the cloud document
		}
	}
#endif
}

void AEditModelGameMode::Logout(AController* Exiting)
{
#if UE_SERVER
	APlayerState* playerState = Exiting ? Exiting->PlayerState : nullptr;
	int32 playerID = playerState ? playerState->GetPlayerId() : INDEX_NONE;

	FString userID;
	if (UsersByPlayerID.RemoveAndCopyValue(playerID, userID))
	{
		PlayersByUserID.Remove(userID);
	}

	// Once the user has logged out, let the multiplayer controller and AMS know that the user is no longer connected to the multiplayer server.
	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (cloudConnection.IsValid())
	{
		if (!cloudConnection->ClearEncryptionKey(userID, CurProjectID))
		{
			UE_LOG(LogGameMode, Error, TEXT("Failed to remove encryption key for User ID %s and Project ID %s"), *userID, *CurProjectID);
		}

		FString deleteConnectionEndpoint = FProjectConnectionHelpers::MakeProjectConnectionEndpoint(CurProjectID) / userID;
		cloudConnection->RequestEndpoint(deleteConnectionEndpoint, FModumateCloudConnection::ERequestType::Delete, nullptr, nullptr, nullptr);
	}

	if (PlayersByUserID.Num() == 0)
	{
		ResetSession();
	}
#endif

	Super::Logout(Exiting);
}

const FString& AEditModelGameMode::GetCurProjectID() const
{
	return CurProjectID;
}

bool AEditModelGameMode::GetUserByPlayerID(int32 PlayerID, FString& OutUserID) const
{
	const FString* userIDPtr = UsersByPlayerID.Find(PlayerID);
	if (userIDPtr)
	{
		OutUserID = *userIDPtr;
		return true;
	}
	else
	{
		return false;
	}
}

void AEditModelGameMode::OnServerStatusTimer()
{
#if UE_SERVER
	if (PlayersByUserID.Num() == 0)
	{
		TimeWithoutPlayers += ServerStatusTimerRate;

		if (TimeWithoutPlayers >= ServerAutoExitTimeout)
		{
			UE_LOG(LogGameMode, Log, TEXT("It has been %.1fsec without any connected players; exiting!"), TimeWithoutPlayers);
			FPlatformMisc::RequestExit(false);
		}
	}
	else
	{
		TimeWithoutPlayers = 0.0f;
	}
#endif
}

void AEditModelGameMode::ResetSession()
{
	UWorld* world = GetWorld();
	auto* gameState = Cast<AEditModelGameState>(GameState);
	if (ensure(world && gameState && gameState->Document))
	{
		gameState->Document->MakeNew(world);
	}
}

#undef LOCTEXT_NAMESPACE
