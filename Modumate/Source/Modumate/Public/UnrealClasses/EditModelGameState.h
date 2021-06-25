// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/ModumateDocument.h"
#include "GameFramework/GameStateBase.h"
#include "Interfaces/IHttpRequest.h"

#include "EditModelGameState.generated.h"

UCLASS(config=Game)
class MODUMATE_API AEditModelGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitDocument(const FString& LocalUserID, int32 LocalUserIdx);

	UPROPERTY()
	UModumateDocument* Document;

	UFUNCTION(NetMulticast, Reliable)
	void BroadcastServerDeltas(const FDeltasRecord& Deltas);

	UFUNCTION(NetMulticast, Reliable)
	void BroadcastUndo(const FString& UndoingUserID, const TArray<uint32>& UndoRecordHashes);

	UFUNCTION()
	bool GetDeltaRecordsSinceSave(TArray<FDeltasRecord>& OutRecordsSinceSave) const;

	UFUNCTION()
	bool DownloadDocument();

	UFUNCTION()
	bool UploadDocument();

	// Sent from the client to the server, if they want to upload the document without waiting for auto-upload
	UFUNCTION(Server, Reliable)
	void RequestImmediateUpload();

	UPROPERTY()
	bool bDownloadingDocument = false;

	UPROPERTY()
	bool bUploadingDocument = false;

	// The document hash that was the most recent one saved by the server
	UPROPERTY()
	uint32 LastUploadedDocHash = 0;

	UPROPERTY()
	uint32 PendingUploadDocHash = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LastUploadTime)
	FDateTime LastUploadTime;

	UPROPERTY(Config)
	float AutoUploadTimerPeriod = 0.25f;

	// How long after performing an action (broadcasting deltas, undoing, etc.) to do an auto-upload from the server
	UPROPERTY(Config)
	float PostActionUploadTime = 5.0f;

	// How many actions can have happened before we force an auto-upload
	UPROPERTY(Config)
	int32 MaxUnsavedActions = 8;

	UPROPERTY()
	class AEditModelGameMode* EditModelGameMode;

protected:
	int32 NumUnsavedActions = 0;
	float TimeSinceAction = 0.0f;
	FTimerHandle AutoUploadTimer;

	void OnPerformedSaveableAction();

	UFUNCTION()
	void OnRep_LastUploadTime();

	UFUNCTION()
	void OnAutoUploadTimer();

	UFUNCTION()
	void OnUploadedDocument(const FString& ProjectID, bool bSuccess);
};
