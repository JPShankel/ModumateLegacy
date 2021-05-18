// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameMode.h"

#include "HttpManager.h"
#include "Kismet/GameplayStatics.h"
#include "Online/ModumateCloudConnection.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"

#define LOCTEXT_NAMESPACE "ModumateOnline"

const FString AEditModelGameMode::OptionKeyID(TEXT("MDID"));
const FString AEditModelGameMode::OptionKeyAuth(TEXT("MDAuth"));

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
	if (ensure(gameState))
	{
		gameState->InitDocument();
	}
}

void AEditModelGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// Non-dedicated servers can just use default behavior; no one should be logging into them.
#if !UE_SERVER
	return;
#endif

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

	// Get the Modumate account ID and auth token from the login options
	// TODO: get these values in a more officially supported way, potentially from the controller's NetUniqueID.
	FString loginID = FPlatformHttp::UrlDecode(UGameplayStatics::ParseOption(Options, OptionKeyID));
	FString loginAuth = FPlatformHttp::UrlDecode(UGameplayStatics::ParseOption(Options, OptionKeyAuth));

	if (loginID.IsEmpty() || loginAuth.IsEmpty())
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid loginID (%s) and/or loginAuth (%s)"), *loginID, *loginAuth);
		ErrorMessage = FString(TEXT("Missing login info; please log in with user ID and authentication."));
		return;
	}

	UE_LOG(LogGameMode, Log, TEXT("Approving Modumate login from user %s..."), *loginID);

	if (PendingStatusIDs.Contains(loginID))
	{
		UE_LOG(LogGameMode, Error, TEXT("UserID %s is already logging in!"), *loginID);
		ErrorMessage = FString(TEXT("Duplicate login; please try again later."));
		return;
	}

	if (VerifiedUserStatuses.Contains(loginID))
	{
		UE_LOG(LogGameMode, Error, TEXT("UserID %s is already logged in!"), *loginID);
		ErrorMessage = FString(TEXT("User is already logged in; please log in as a different user."));
		return;
	}

	PendingStatusIDs.Add(loginID);

	TWeakObjectPtr<AEditModelGameMode> weakThis(this);
	cloudConnection->SetAuthToken(loginAuth);

	// Make a manual request to our AMS to verify that the user is allowed to communicate with this server.
	// TODO: use a different endpoint besides "/status" to verify workspace and read/write permissions; currently we only verify that the user exists.
	auto statusRequest = cloudConnection->MakeRequest(
		[weakThis, loginID](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (!weakThis.IsValid())
			{
				return;
			}

			weakThis->PendingStatusIDs.Remove(loginID);

			FModumateUserStatus status;
			if (bSuccessful && Response.IsValid() &&
				FJsonObjectConverter::JsonObjectToUStruct<FModumateUserStatus>(Response.ToSharedRef(), &status))
			{
				UE_LOG(LogGameMode, Log, TEXT("Received status for user: %s"), *loginID);
				weakThis->VerifiedUserStatuses.Add(loginID, status);
			}
		},
		[weakThis, loginID](int32 ErrorCode, const FString& ErrorString)
		{
			UE_LOG(LogGameMode, Error, TEXT("Request Status Error: %s"), *ErrorString);
			if (weakThis.IsValid())
			{
				weakThis->PendingStatusIDs.Remove(loginID);
				weakThis->LoginErrors.Add(loginID, ErrorString);
			}
		},
			false);

	static const FString statusEndpoint(TEXT("status"));
	static const float statusServerTimeout = 4.0f;
	static const float statusTickTime = 0.01f;

	statusRequest->SetURL(cloudConnection->GetCloudAPIURL() / statusEndpoint);
	statusRequest->SetVerb(FModumateCloudConnection::GetRequestTypeString(FModumateCloudConnection::Get));
	statusRequest->SetTimeout(statusServerTimeout);
	if (!statusRequest->ProcessRequest())
	{
		UE_LOG(LogGameMode, Error, TEXT("Couldn't start login request!"));
		ErrorMessage = FString(TEXT("Server login error; please try again later."));
		return;
	}

	// HACK: without creating our own OnlineSubsystem, relying on AModumateGameSession::ApproveLogin
	// means we're forced to wait for the AMS in a blocking fashion.
	EHttpRequestStatus::Type requestStatus = EHttpRequestStatus::NotStarted;
	int32 responseCode = 0;
	while ((statusRequest->GetElapsedTime() < statusServerTimeout) &&
		!EHttpRequestStatus::IsFinished(requestStatus) && (responseCode == 0))
	{
		FPlatformProcess::Sleep(statusTickTime);
		FHttpModule::Get().GetHttpManager().Tick(statusTickTime);
		requestStatus = statusRequest->GetStatus();
		auto response = statusRequest->GetResponse();
		if (response.IsValid())
		{
			responseCode = response->GetResponseCode();
		}
	}

	if (!VerifiedUserStatuses.Contains(loginID) || !VerifiedUserStatuses[loginID].Active)
	{
		FString error = LoginErrors.FindRef(loginID);
		UE_LOG(LogGameMode, Error, TEXT("UserID %s login error: %s"), *loginID, *error);
		ErrorMessage = FString::Printf(TEXT("Log in failure: %s"), *error);
		return;
	}

	UE_LOG(LogGameMode, Log, TEXT("UserID %s login approved!"), *loginID);
}

APlayerController* AEditModelGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	APlayerController* newController = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);

	FString loginID = FPlatformHttp::UrlDecode(UGameplayStatics::ParseOption(Options, OptionKeyID));

	APlayerState* playerState = newController ? newController->PlayerState : nullptr;
	int32 playerID = playerState ? playerState->GetPlayerId() : INDEX_NONE;
	if ((playerID == INDEX_NONE) || UsersByPlayerID.Contains(playerID) || loginID.IsEmpty())
	{
		UE_LOG(LogGameMode, Error, TEXT("Invalid/duplicate playerID: %d"), playerID);
		ErrorMessage = FString(TEXT("Log in failure"));
		return newController;
	}

	UsersByPlayerID.Add(playerID, loginID);

	FModumateUserStatus* userStatus = VerifiedUserStatuses.Find(loginID);
	if (userStatus)
	{
		ChangeName(newController, loginID, true);
	}

	return newController;
}

void AEditModelGameMode::Logout(AController* Exiting)
{
	APlayerState* playerState = Exiting ? Exiting->PlayerState : nullptr;
	int32 playerID = playerState ? playerState->GetPlayerId() : INDEX_NONE;

	FString userID;
	if (UsersByPlayerID.RemoveAndCopyValue(playerID, userID))
	{
		PendingStatusIDs.Remove(userID);
		VerifiedUserStatuses.Remove(userID);
		LoginErrors.Remove(userID);
	}

	Super::Logout(Exiting);
}

#undef LOCTEXT_NAMESPACE
