// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameMode.h"

#include "DocumentManagement/ModumateDocument.h"
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
const FString AEditModelGameMode::CloudUrl(TEXT("-ModAMSURL="));

extern TAutoConsoleVariable<FString> CVarModumateCloudAddress;

AEditModelGameMode::AEditModelGameMode()
	: Super()
{
	PlayerStateClass = AEditModelPlayerState::StaticClass();
	DynamicMeshActorClass = ADynamicMeshActor::StaticClass();
}

void AEditModelGameMode::InitGameState()
{
	Super::InitGameState();

	auto* world = GetWorld();
	auto* gameState = Cast<AEditModelGameState>(GameState);
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;

	if (!ensure(world && gameState && accountManager.IsValid() && cloudConnection.IsValid()))
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
	if (GameSession)
	{
		GameSession->MaxPlayers = UModumateDocument::MaxUserIdx;
	}

	FString inXApiKey;
	if (!FParse::Value(FCommandLine::Get(), *AEditModelGameMode::ProjectIDArg, CurProjectID) || CurProjectID.IsEmpty() ||
		!FParse::Value(FCommandLine::Get(), *AEditModelGameMode::APIKeyArg, inXApiKey) || inXApiKey.IsEmpty())
	{
		UE_LOG(LogGameMode, Fatal, TEXT("Can't start an EditModelGameMode server without %s and %s arguments!"), *AEditModelGameMode::ProjectIDArg, *AEditModelGameMode::APIKeyArg);
		FPlatformMisc::RequestExit(false);
		return;
	}

	FString cloudUrl;
	if (FParse::Value(FCommandLine::Get(), *CloudUrl, cloudUrl))
	{
		UE_LOG(LogGameMode, Log, TEXT("Overriding cloud URL to %s"), *cloudUrl);
		CVarModumateCloudAddress->Set(*cloudUrl, EConsoleVariableFlags::ECVF_SetByCommandline);
	}

	cloudConnection->SetXApiKey(inXApiKey);

	FString redactedKey = FString::Printf(TEXT("%c****%c"), inXApiKey[0], inXApiKey[inXApiKey.Len() - 1]);
	UE_LOG(LogGameMode, Log, TEXT("Starting EditModelGameMode server with Project ID: %s, API Key: %s"), *CurProjectID, *redactedKey);

	gameState->Document->MakeNew(world);
	gameState->DownloadDocument(CurProjectID);

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
	auto* gameState = Cast<AEditModelGameState>(GameState);
	if (!ensure(cloudConnection.IsValid() && gameState && gameState->Document))
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid ModumateCloudConnection and/or Document!"));
		ErrorMessage = FString(TEXT("Server is not ready; please try again later."));
		return;
	}

	FString userID, projectID, encryptionKey;
	if (!ensure(FModumateCloudConnection::ParseConnectionOptions(Options, userID, projectID)))
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid connection options: %s"), *Options);
		ErrorMessage = FString(TEXT("Log in failure"));
		return;
	}

	// Now, get the encryption key for the encryption token, which should already be cached on the CloudConnection,
	// assuming the user already successfully shook hands via our encrypted packet handler.
	if (!cloudConnection->GetCachedEncryptionKey(userID, projectID, encryptionKey))
	{
		UE_LOG(LogGameMode, Error, TEXT("Couldn't get encryption key for User ID: %s"), *userID);
		ErrorMessage = FString(TEXT("Login failed; check credentials and try again later."));
		return;
	}

	if (projectID != CurProjectID)
	{
		UE_LOG(LogGameMode, Error, TEXT("Input project ID \"%s\" doesn't match current project ID: %s"), *projectID, *CurProjectID);
		ErrorMessage = FString(TEXT("Invalid session ID; make sure you've selected the correct server and try again."));
		return;
	}

	if (OrderedUserIDs.Num() >= UModumateDocument::MaxUserIdx)
	{
		UE_LOG(LogGameMode, Error, TEXT("Player cap (%d) reached!"), UModumateDocument::MaxUserIdx);
		ErrorMessage = FString::Printf(TEXT("Only %d users can connect to the server at a time - please try again later after someone disconnects."), UModumateDocument::MaxUserIdx);
		return;
	}

	if (PendingLoginDocHashByUserID.Contains(userID))
	{
		UE_LOG(LogGameMode, Error, TEXT("UserID %s is already in the process of logging in!"), *userID);
		ErrorMessage = FString(TEXT("Duplicate login; please try again later."));
		return;
	}

	if (PlayersByUserID.Contains(userID))
	{
		UE_LOG(LogGameMode, Error, TEXT("UserID %s is already logged in!"), *userID);
		ErrorMessage = FString(TEXT("Duplicate login; please try again later."));
		return;
	}

	UE_LOG(LogGameMode, Log, TEXT("UserID %s login approved!"), *userID);
	PlayersByUserID.Add(userID, INDEX_NONE);

	uint32 latestDocHash = gameState->Document->GetLatestVerifiedDocHash();
	PendingLoginDocHashByUserID.Add(userID, latestDocHash);

	// Now that we're going to let the client log in, we need to upload the current version of the document,
	// so that client can download the same version.
	if (gameState->UploadDocument())
	{
		PendingLoginUploadDocHashes.Add(latestDocHash);
	}
#endif
}

APlayerController* AEditModelGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	APlayerController* newController = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);

#if UE_SERVER
	FString userID, projectID;
	if (!ensure(FModumateCloudConnection::ParseConnectionOptions(Options, userID, projectID)))
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid connection options: %s"), *Options);
		ErrorMessage = FString(TEXT("Log in failure"));
		return newController;
	}

	APlayerState* playerState = newController ? newController->PlayerState : nullptr;
	int32 playerID = playerState ? playerState->GetPlayerId() : INDEX_NONE;
	if ((playerID == INDEX_NONE) || UsersByPlayerID.Contains(playerID) || !PlayersByUserID.Contains(userID))
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid/duplicate Player ID: %d and User ID: %s"), playerID, *userID);
		ErrorMessage = FString(TEXT("Log in failure"));
		return newController;
	}

	UsersByPlayerID.Add(playerID, userID);
	PlayersByUserID[userID] = playerID;

	int32 firstEmptyIdx = OrderedUserIDs.Find(FString());
	if (firstEmptyIdx == INDEX_NONE)
	{
		OrderedUserIDs.Add(userID);
	}
	else
	{
		OrderedUserIDs[firstEmptyIdx] = userID;
	}
#endif

	return newController;
}

void AEditModelGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	TrySendUserInitialState(NewPlayer);
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

	if (PlayersByUserID.Num() == 0)
	{
		auto* gameState = Cast<AEditModelGameState>(GameState);
		if (ensure(gameState && gameState->Document))
		{
			gameState->Document->PurgeDeltas();
			gameState->Document->SetDirtyFlags(true);
			gameState->UploadDocument();
		}
	}

	// Clear the User ID from the ordered User IDs, so its index can get reused
	int32 userIdx = userID.IsEmpty() ? INDEX_NONE : OrderedUserIDs.IndexOfByKey(userID);
	if (ensure(userIdx != INDEX_NONE))
	{
		OrderedUserIDs[userIdx].Empty();
	}

	PendingLoginDocHashByUserID.Remove(userID);

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

bool AEditModelGameMode::GetPlayerByUserID(const FString& UserID, int32& OutPlayerID) const
{
	const int32* playerIDPtr = PlayersByUserID.Find(UserID);
	if (playerIDPtr)
	{
		OutPlayerID = *playerIDPtr;
		return true;
	}
	{
		return false;
	}
}

bool AEditModelGameMode::IsAnyUserPendingLogin() const
{
	return PendingLoginDocHashByUserID.Num() > 0;
}

void AEditModelGameMode::OnUserDownloadedDocument(const FString& UserID, uint32 DownloadedDocHash)
{
#if UE_SERVER
	auto* gameState = Cast<AEditModelGameState>(GameState);
	if (!ensure(gameState && gameState->Document))
	{
		return;
	}

	uint32 currentDocHash = gameState->Document->GetLatestVerifiedDocHash();
	bool bDownloadedLatest = (currentDocHash == DownloadedDocHash);

	uint32 pendingDocHash = 0;
	bool bDownloadedPendingDoc = PendingLoginDocHashByUserID.RemoveAndCopyValue(UserID, pendingDocHash) && (pendingDocHash == DownloadedDocHash);

	if (bDownloadedLatest && bDownloadedPendingDoc)
	{
		UE_LOG(LogGameMode, Log, TEXT("User ID %s successfully downloaded current doc hash %08x"), *UserID, currentDocHash);

		if (PendingLoginDocHashByUserID.Num() == 0)
		{
			UE_LOG(LogGameMode, Log, TEXT("No users are currently logging in, operations can safely resume!"));
		}
	}
	else
	{
		UE_LOG(LogGameMode, Error, TEXT("User ID %s reportedly finished downloading their document (hash %08x), but it wasn't the correct version!"), *UserID, DownloadedDocHash);

		APlayerController* userController = FindControllerByUserID(UserID);
		if (ensure(GameSession) && userController)
		{
			GameSession->KickPlayer(userController, LOCTEXT("ClientBadDownload", "There was an error downloading the project, please try again later."));
		}
	}
#endif
}

void AEditModelGameMode::OnServerUploadedDocument(uint32 UploadedDocHash)
{
#if UE_SERVER
	if (PendingLoginUploadDocHashes.Contains(UploadedDocHash))
	{
		PendingLoginUploadDocHashes.Remove(UploadedDocHash);

		// If we just finished uploading a document, but a player has already logged in and needs to know what document to download,
		// we can let them know here.
		for (auto& kvp : PendingLoginDocHashByUserID)
		{
			if (kvp.Value == UploadedDocHash)
			{
				auto* controller = FindControllerByUserID(kvp.Key);
				if (controller && controller->GetPawn())
				{
					TrySendUserInitialState(controller);
				}
			}
		}
	}
#endif
}

APlayerController* AEditModelGameMode::FindControllerByUserID(const FString& UserID)
{
	if (!PlayersByUserID.Contains(UserID) || (GameState == nullptr))
	{
		return nullptr;
	}

	for (APlayerState* player : GameState->PlayerArray)
	{
		auto* controller = player->GetOwner<APlayerController>();
		if ((player->GetPlayerId() == PlayersByUserID[UserID]) && controller)
		{
			return controller;
		}
	}

	return nullptr;
}

bool AEditModelGameMode::TrySendUserInitialState(APlayerController* NewPlayer)
{
#if UE_SERVER
	auto* playerState = NewPlayer ? Cast<AEditModelPlayerState>(NewPlayer->PlayerState) : nullptr;
	auto* playerPawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	int32 playerID = playerState ? playerState->GetPlayerId() : INDEX_NONE;
	FString userID = UsersByPlayerID.FindRef(playerID);
	int32 userIdx = userID.IsEmpty() ? INDEX_NONE : OrderedUserIDs.IndexOfByKey(userID);
	auto* gameState = Cast<AEditModelGameState>(GameState);
	if (ensure(playerState && playerPawn && gameState && !userID.IsEmpty() && (userIdx != INDEX_NONE) &&
		(userIdx < UModumateDocument::MaxUserIdx) && PendingLoginDocHashByUserID.Contains(userID)))
	{
		uint32 userDocHash = PendingLoginDocHashByUserID[userID];
		if (PendingLoginUploadDocHashes.Contains(userDocHash))
		{
			UE_LOG(LogGameMode, Warning, TEXT("User ID %s finished logging in, but we're still uploading the document (hash %08x) for them to download. Deferring sending initial state..."),
				*userID, userDocHash);
		}
		else
		{
			playerState->SendInitialState(CurProjectID, userIdx, userDocHash);
			return true;
		}
	}
	else
	{
		UE_LOG(LogGameMode, Error, TEXT("Unknown login failure! User ID %s, Player ID %d, User #%d"), *userID, playerID, userIdx);
		GameSession->KickPlayer(NewPlayer, LOCTEXT("LoginFailureKick", "You have failed to connect to the server, please try again."));
	}
#endif

	return false;
}

void AEditModelGameMode::OnServerStatusTimer()
{
#if UE_SERVER
	// Allow skipping the auto-exit with the "longtimeouts" parameter, which is also how we enable longer connection timeout values.
	static bool bShouldAutoExit = !FParse::Param(FCommandLine::Get(), TEXT("longtimeouts"));

	if (PlayersByUserID.Num() == 0 || GetNumPlayers() == 0)
	{
		TimeWithoutPlayers += ServerStatusTimerRate;

		if (bShouldAutoExit && (TimeWithoutPlayers >= ServerAutoExitTimeout))
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

#undef LOCTEXT_NAMESPACE
