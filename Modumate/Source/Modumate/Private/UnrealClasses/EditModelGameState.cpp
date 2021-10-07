// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameState.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "Net/UnrealNetwork.h"
#include "Online/ModumateCloudConnection.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"

#define LOCTEXT_NAMESPACE "EditModelGameState"

void AEditModelGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEditModelGameState, LastUploadedDocHash);
	DOREPLIFETIME(AEditModelGameState, LastUploadTime);
}

void AEditModelGameState::InitDocument(const FString& LocalUserID, int32 LocalUserIdx)
{
	if (ensure(Document == nullptr))
	{
		Document = NewObject<UModumateDocument>(this);
		Document->SetLocalUserInfo(LocalUserID, LocalUserIdx);

		LastUploadedDocHash = 0;
		LastUploadTime = FDateTime::UtcNow();

#if UE_SERVER
		GetWorld()->GetTimerManager().SetTimer(AutoUploadTimer, this, &AEditModelGameState::OnAutoUploadTimer, AutoUploadTimerPeriod, true);
#endif

		// Only expected to be non-null on offline clients and dedicated servers
		UWorld* world = GetWorld();
		EditModelGameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
	}
}

// RPC from the server to every copy of the global GameState
void AEditModelGameState::BroadcastServerDeltas_Implementation(const FString& SourceUserID, const FDeltasRecord& Deltas, bool bRedoingRecord)
{
	UWorld* world = GetWorld();
	if (ensure(world && Document))
	{
		Document->ApplyRemoteDeltas(Deltas, world, bRedoingRecord);

#if UE_SERVER
		// On the server, use this opportunity to potentially wipe every player's redo stack.
		int32 originPlayerID = 0;
		if (ensure(EditModelGameMode && EditModelGameMode->GetPlayerByUserID(SourceUserID, originPlayerID)))
		{
			for (auto* playerState : PlayerArray)
			{
				AEditModelPlayerState* emPlayerState = Cast<AEditModelPlayerState>(playerState);
				if (emPlayerState && (emPlayerState->GetPlayerId() != originPlayerID))
				{
					emPlayerState->ClearConflictingRedoBuffer(Deltas);
				}
			}
		}
#endif
	}
}

// RPC from the server to every copy of the global GameState
void AEditModelGameState::BroadcastUndo_Implementation(const FString& UndoingUserID, const TArray<uint32>& UndoRecordHashes)
{
	UWorld* world = GetWorld();
	if (ensure(world && Document))
	{
		Document->ApplyRemoteUndo(world, UndoingUserID, UndoRecordHashes);
	}
}

bool AEditModelGameState::DownloadDocument(const FString& ProjectID)
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	TSharedPtr<FModumateCloudConnection> cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;

	if (!ensure(Document && cloudConnection.IsValid() && !bDownloadingDocument &&
		CurProjectID.IsEmpty() && !ProjectID.IsEmpty()))
	{
		return false;
	}

	// Assign CurProjectID here, since downloading the document is the first thing that happens
	// for both clients and servers in multiplayer sessions.
	CurProjectID = ProjectID;

	auto weakThis = MakeWeakObjectPtr<AEditModelGameState>(this);
	bDownloadingDocument = cloudConnection->DownloadProject(CurProjectID,
		[weakThis](const FModumateDocumentHeader& DocHeader, const FMOIDocumentRecord& DocRecord, bool bEmpty) {
			if (ensure(weakThis.IsValid()))
			{
				weakThis->OnDownloadDocumentSuccess(DocHeader, DocRecord, bEmpty);
			}
		},
		[weakThis](int32 ErrorCode, const FString& ErrorMessage) {
			if (ensure(weakThis.IsValid()))
			{
				weakThis->OnDownloadDocumentFailure(ErrorCode, ErrorMessage);
			}
		});

	bool bRequestedInfo = cloudConnection->RequestEndpoint(
		FProjectConnectionHelpers::MakeProjectInfoEndpoint(CurProjectID),
		FModumateCloudConnection::ERequestType::Get,
		nullptr,
		[weakThis](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			FProjectInfoResponse projectInfo;
			if (ensure(weakThis.IsValid() && bSuccessful && Response.IsValid() &&
				FJsonObjectConverter::JsonObjectToUStruct(Response.ToSharedRef(), &projectInfo)))
			{
				weakThis->OnReceivedProjectInfo(projectInfo);
			}
		},
		[weakThis](int32 ErrorCode, const FString& ErrorMessage) {
			UE_LOG(LogGameState, Error, TEXT("Error code %d querying info for Project ID %s: %s"),
				ErrorCode, weakThis.IsValid() ? *weakThis->CurProjectID : TEXT("?"), *ErrorMessage);
		});

	return bDownloadingDocument;
}

bool AEditModelGameState::UploadDocument()
{
	if (!ensure(Document && !CurProjectID.IsEmpty()))
	{
		return false;
	}

	uint32 curDocHash = Document->GetLatestVerifiedDocHash();
	auto* world = GetWorld();
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	TSharedPtr<FModumateCloudConnection> cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	FModumateDocumentHeader docHeader;
	FMOIDocumentRecord docRecord;
	TArray<uint8> docBuffer;

	if (!Document->IsDirty(false) || CurrentUploadHashes.Contains(curDocHash) ||
		!ensure(cloudConnection.IsValid() && Document->SerializeRecords(world, docHeader, docRecord)))
	{
		return false;
	}

	auto weakThis = MakeWeakObjectPtr<AEditModelGameState>(this);
	bool bRequestSuccess = cloudConnection->UploadProject(CurProjectID, docHeader, docRecord,
		[weakThis, curDocHash](bool bSuccessful, const TSharedPtr<FJsonObject>& Response) {
			if (ensure(weakThis.IsValid() && bSuccessful))
			{
				weakThis->OnUploadedDocument(true, curDocHash);
			}
		},
		[weakThis, curDocHash](int32 ErrorCode, const FString& ErrorMessage) {
			UE_LOG(LogGameState, Error, TEXT("Error code %d when trying to upload Project ID %s: %s"),
				ErrorCode, weakThis.IsValid() ? *weakThis->CurProjectID : TEXT("?"), *ErrorMessage);
			if (ensure(weakThis.IsValid()))
			{
				weakThis->OnUploadedDocument(false, curDocHash);
			}
		});

	if (bRequestSuccess)
	{
		CurrentUploadHashes.Add(curDocHash);
	}

	return bRequestSuccess;
}

bool AEditModelGameState::UploadThumbnail(const TArray<uint8>& ThumbImage)
{
	if (!ensure(Document && !CurProjectID.IsEmpty()))
	{
		return false;
	}

	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	TSharedPtr<FModumateCloudConnection> cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (cloudConnection)
	{
		bool bSuccess = cloudConnection->RequestEndpoint(FProjectConnectionHelpers::MakeProjectThumbnailEndpoint(CurProjectID), FModumateCloudConnection::Post,
			[&ThumbImage](FHttpRequestRef& RefRequest)
			{
				RefRequest->SetHeader(TEXT("Content-Type"), TEXT("image/jpeg"));
				RefRequest->SetContent(ThumbImage);
			},
			[](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
			{
				if (!bSuccessful || !Response.IsValid())
				{
					UE_LOG(LogGameState, Error, TEXT("Thumbnail upload rejected"));
				}
			},
			[](int32 ErrorCode, const FString& ErrorMessage)
			{
				UE_LOG(LogGameState, Error, TEXT("Thumbnail upload failed (%d): %s"), ErrorCode, *ErrorMessage);
			}
		);
		return bSuccess;
	}

	return false;
}

// Takes the users CloudID and provides their corresponding PlayersState.
// String searched are slow. Doing this every frame is not recommended.
AEditModelPlayerState* AEditModelGameState::GetPlayerFromCloudID(const FString& Id) const
{
	for (const auto& curPS : PlayerArray)
	{
		if (curPS != nullptr)
		{
			AEditModelPlayerState* emPS = Cast<AEditModelPlayerState>(curPS);
			FString cloudID = emPS->ReplicatedUserInfo.ID;

			if (cloudID.Equals(Id))
			{
				return emPS;
			}
		}
	}

	return nullptr;
}

void AEditModelGameState::OnRep_LastUploadedDocHash()
{
	if (IsNetMode(NM_Client) && Document && Document->IsDirty(true) &&
		(Document->GetLatestVerifiedDocHash() == LastUploadedDocHash))
	{
		Document->SetDirtyFlags(false);
	}
}

void AEditModelGameState::OnRep_LastUploadTime()
{
}

void AEditModelGameState::OnAutoUploadTimer()
{
#if UE_SERVER
	if (Document == nullptr)
	{
		return;
	}

	if (Document->IsDirty(false))
	{
		TimeSinceDirty += AutoUploadTimerPeriod;

		if (TimeSinceDirty > PostDirtyUploadTime)
		{
			UploadDocument();
			TimeSinceDirty = 0.0f;
		}
	}
	else
	{
		TimeSinceDirty = 0.0f;
	}
#endif
}

void AEditModelGameState::OnDownloadDocumentSuccess(const FModumateDocumentHeader& DocHeader, const FMOIDocumentRecord& DocRecord, bool bEmpty)
{
	UWorld* world = GetWorld();
	if (!ensure(world && Document && !CurProjectID.IsEmpty()))
	{
		OnDownloadDocumentFailure(0, FString(TEXT("Haven't initialized Document or been given a Project ID before download completed!")));
		return;
	}

	bDownloadingDocument = false;
	bool bLoadedProject = (bEmpty || Document->LoadRecord(world, DocHeader, DocRecord, false));
	uint32 loadedDocHash = Document->GetLatestVerifiedDocHash();
	LastUploadedDocHash = loadedDocHash;
	LastUploadTime = FDateTime::UtcNow();
	UE_LOG(LogGameState, Log, TEXT("Downloaded Project ID %s is on hash %08x"), *CurProjectID, loadedDocHash);

	// If we're a local multiplayer client, then we're expecting to finish downloading the pending document before we can perform any actions,
	// and we'll also need to let the server know that we've finished downloading, so we can safely receive RPCs and all clients can send them.
	if (IsNetMode(NM_Client))
	{
		ULocalPlayer* localPlayer = world->GetFirstLocalPlayerFromController();
		auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(world)) : nullptr;
		if (ensure(controller))
		{
			controller->OnDownloadedClientDocument(loadedDocHash);
		}
	}
	// If we're a multiplayer server and we just downloaded a document that's already dirty,
	// then we should try to upload it immediately, so that any joining clients have a chance to get the cleaned version.
	else if (IsNetMode(NM_DedicatedServer) && Document->IsDirty(false))
	{
		UploadDocument();
	}
}

void AEditModelGameState::OnDownloadDocumentFailure(int32 ErrorCode, const FString& ErrorMessage)
{
	UE_LOG(LogGameState, Error, TEXT("Error code %d when trying to download Project ID %s: %s"), ErrorCode, *CurProjectID, *ErrorMessage);
	bDownloadingDocument = false;

#if UE_SERVER
	// Servers only download documents upon initialization, and cannot tolerate failure.
	FPlatformMisc::RequestExit(false);
#else
	// Clients can fail more gracefully, and go back to the main menu if the project they intended to download was unsuccessful.
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (ensure(gameInstance))
	{
		static const FString eventName(TEXT("ErrorClientDocumentDownload"));
		UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Network, eventName);
		gameInstance->GoToMainMenu(LOCTEXT("DocDownloadFailure", "Failed to download project, please try again later."));
	}
#endif
}

void AEditModelGameState::OnUploadedDocument(bool bSuccess, uint32 UploadedDocHash)
{
	if (bSuccess && CurrentUploadHashes.Contains(UploadedDocHash))
	{
		UE_LOG(LogGameState, Log, TEXT("Successfully uploaded Project ID: %s, hash %08x"), *CurProjectID, UploadedDocHash);
		LastUploadedDocHash = UploadedDocHash;
		LastUploadTime = FDateTime::UtcNow();

		// Clear the dirty flag if we just uploaded the document's current hash
		if (Document && (Document->GetLatestVerifiedDocHash() == UploadedDocHash))
		{
			Document->SetDirtyFlags(false);
		}
	}
	
	CurrentUploadHashes.Remove(UploadedDocHash);

	if (EditModelGameMode)
	{
		EditModelGameMode->OnServerUploadedDocument(UploadedDocHash);
	}
}

void AEditModelGameState::OnReceivedProjectInfo(const FProjectInfoResponse& InProjectInfo)
{
	CurProjectInfo = InProjectInfo;

	if (Document && !CurProjectInfo.Name.IsEmpty())
	{
		Document->SetCurrentProjectName(CurProjectInfo.Name, false);
	}
}

#undef LOCTEXT_NAMESPACE
