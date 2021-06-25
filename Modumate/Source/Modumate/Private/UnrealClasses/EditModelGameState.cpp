// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameState.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "Net/UnrealNetwork.h"
#include "Online/ModumateCloudConnection.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/ModumateGameInstance.h"


void AEditModelGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEditModelGameState, LastUploadTime);
}

void AEditModelGameState::InitDocument(const FString& LocalUserID, int32 LocalUserIdx)
{
	if (ensure(Document == nullptr))
	{
		Document = NewObject<UModumateDocument>(this);
		Document->SetLocalUserInfo(LocalUserID, LocalUserIdx);

		LastUploadedDocHash = 0;

#if UE_SERVER
		GetWorld()->GetTimerManager().SetTimer(AutoUploadTimer, this, &AEditModelGameState::OnAutoUploadTimer, AutoUploadTimerPeriod, true);
#endif

		// Only expected to be non-null on offline clients and dedicated servers
		UWorld* world = GetWorld();
		EditModelGameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
	}
}

// RPC from the server to every client's copy of the global GameState
void AEditModelGameState::BroadcastServerDeltas_Implementation(const FDeltasRecord& Deltas)
{
	UWorld* world = GetWorld();
	if (ensure(world && Document))
	{
		Document->ApplyRemoteDeltas(Deltas, world, false);
		OnPerformedSaveableAction();
	}
}

// RPC from the server to every client's copy of the global GameState
void AEditModelGameState::BroadcastUndo_Implementation(const FString& UndoingUserID, const TArray<uint32>& UndoRecordHashes)
{
	UWorld* world = GetWorld();
	if (ensure(world && Document))
	{
		Document->ApplyRemoteUndo(world, UndoingUserID, UndoRecordHashes);
		OnPerformedSaveableAction();
	}
}

bool AEditModelGameState::GetDeltaRecordsSinceSave(TArray<FDeltasRecord>& OutRecordsSinceSave) const
{
	OutRecordsSinceSave.Reset();

	if (!ensure(Document))
	{
		return false;
	}

	auto& verifiedRecords = Document->GetVerifiedDeltasRecords();

	// If we don't have any DeltasRecords, then there are none to report
	if (verifiedRecords.Num() == 0)
	{
		return true;
	}

	// Otherwise, try to find the last saved record
	int32 lastSavedRecordIdx = verifiedRecords.IndexOfByPredicate([this](const FDeltasRecord& VerifiedDeltasRecord)
		{ return (VerifiedDeltasRecord.TotalHash == LastUploadedDocHash); });

	// If we're unsuccessful, then we don't have a way of reporting a correct set of deltas to apply on top of the saved document
	if (lastSavedRecordIdx == INDEX_NONE)
	{
		return false;
	}

	for (int32 i = lastSavedRecordIdx + 1; i < verifiedRecords.Num(); ++i)
	{
		OutRecordsSinceSave.Add(verifiedRecords[i]);
	}

	return true;
}

bool AEditModelGameState::DownloadDocument()
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	TSharedPtr<FModumateCloudConnection> cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;

	if (!ensure(EditModelGameMode && Document && cloudConnection.IsValid() && !bDownloadingDocument))
	{
		return false;
	}

	const FString& projectID = EditModelGameMode->GetCurProjectID();

	auto weakThis = MakeWeakObjectPtr<AEditModelGameState>(this);
	bDownloadingDocument = cloudConnection->DownloadProject(projectID,
		[weakThis, projectID](const FModumateDocumentHeader& DocHeader, const FMOIDocumentRecord& DocRecord, bool bEmpty) {
			if (ensure(weakThis.IsValid() && weakThis->EditModelGameMode))
			{
				bool bLoadedProject = weakThis->Document && (bEmpty || weakThis->Document->LoadRecord(weakThis->GetWorld(), DocHeader, DocRecord));
				weakThis->LastUploadedDocHash = bLoadedProject ? weakThis->Document->GetLatestVerifiedDocHash() : 0;
				weakThis->bDownloadingDocument = false;
			}
		},
		[weakThis, projectID](int32 ErrorCode, const FString& ErrorMessage) {
			UE_LOG(LogTemp, Fatal, TEXT("Error code %d when trying to download Project ID %s: %s"), ErrorCode, *projectID, *ErrorMessage);
			FPlatformMisc::RequestExit(false);
		});

	return bDownloadingDocument;
}

bool AEditModelGameState::UploadDocument()
{
	auto* world = GetWorld();
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	TSharedPtr<FModumateCloudConnection> cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	FModumateDocumentHeader docHeader;
	FMOIDocumentRecord docRecord;
	TArray<uint8> docBuffer;

	if ((NumUnsavedActions == 0) || bUploadingDocument ||
		!ensure(EditModelGameMode && Document && cloudConnection.IsValid() && Document->SerializeRecords(world, docHeader, docRecord)))
	{
		return false;
	}

	auto weakThis = MakeWeakObjectPtr<AEditModelGameState>(this);
	const FString& projectID = EditModelGameMode->GetCurProjectID();
	bool bRequestSuccess = cloudConnection->UploadProject(projectID, docHeader, docRecord,
		[weakThis, projectID](bool bSuccessful, const TSharedPtr<FJsonObject>& Response) {
			if (ensure(weakThis.IsValid() && bSuccessful))
			{
				weakThis->OnUploadedDocument(projectID, true);
			}
		},
		[weakThis, projectID](int32 ErrorCode, const FString& ErrorMessage) {
			UE_LOG(LogTemp, Error, TEXT("Error code %d when trying to upload Project ID %s: %s"), ErrorCode, *projectID, *ErrorMessage);
			if (ensure(weakThis.IsValid()))
			{
				weakThis->OnUploadedDocument(projectID, false);
			}
		});

	bUploadingDocument = bRequestSuccess;
	PendingUploadDocHash = bRequestSuccess ? Document->GetLatestVerifiedDocHash() : 0;
	if (bRequestSuccess)
	{
		NumUnsavedActions = 0;
	}

	return bRequestSuccess;
}

// RPC from the client to the server's copy of EditModelGameState
void AEditModelGameState::RequestImmediateUpload_Implementation()
{
	UploadDocument();
}

void AEditModelGameState::OnPerformedSaveableAction()
{
	TimeSinceAction = 0.0f;
	if (ensure(Document) && (LastUploadedDocHash == Document->GetLatestVerifiedDocHash()))
	{
		NumUnsavedActions = 0;
	}
	else
	{
		NumUnsavedActions++;
	}
}

void AEditModelGameState::OnRep_LastUploadTime()
{
	UE_LOG(LogTemp, Log, TEXT("Online Document was last saved: %s"), *LastUploadTime.ToString());
}

void AEditModelGameState::OnAutoUploadTimer()
{
#if UE_SERVER
	if (Document == nullptr)
	{
		return;
	}

	TimeSinceAction += AutoUploadTimerPeriod;

	if ((NumUnsavedActions > 0) && ((TimeSinceAction > PostActionUploadTime) || (NumUnsavedActions >= MaxUnsavedActions)))
	{
		UploadDocument();
	}
#endif
}

void AEditModelGameState::OnUploadedDocument(const FString& ProjectID, bool bSuccess)
{
	if (bUploadingDocument && bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully uploaded Project ID: %s"), *ProjectID)
		LastUploadTime = FDateTime::Now();
		LastUploadedDocHash = PendingUploadDocHash;
	}
	
	bUploadingDocument = false;
	PendingUploadDocHash = 0;
}
