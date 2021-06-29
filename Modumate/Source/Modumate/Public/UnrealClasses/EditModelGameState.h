// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/ModumateDocument.h"
#include "GameFramework/GameStateBase.h"
#include "Interfaces/IHttpRequest.h"
#include "Online/ProjectConnection.h"

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
	void BroadcastServerDeltas(const FString& SourceUserID, const FDeltasRecord& Deltas, bool bRedoingRecord = false);

	UFUNCTION(NetMulticast, Reliable)
	void BroadcastUndo(const FString& UndoingUserID, const TArray<uint32>& UndoRecordHashes);

	UFUNCTION()
	bool DownloadDocument(const FString& ProjectID);

	UFUNCTION()
	bool UploadDocument();

	UPROPERTY()
	bool bDownloadingDocument = false;

	// The document hash that was the most recent one saved by the server
	UPROPERTY(ReplicatedUsing = OnRep_LastUploadedDocHash)
	uint32 LastUploadedDocHash = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LastUploadTime)
	FDateTime LastUploadTime = FDateTime(0);

	UPROPERTY(Config)
	float AutoUploadTimerPeriod = 0.25f;

	// How long after the document is detected as dirty should we auto-upload from the server
	UPROPERTY(Config)
	float PostDirtyUploadTime = 5.0f;

	UPROPERTY()
	class AEditModelGameMode* EditModelGameMode;

protected:
	FString CurProjectID;
	FProjectInfoResponse CurProjectInfo;
	TSet<uint32> CurrentUploadHashes;
	float TimeSinceDirty = 0.0f;
	FTimerHandle AutoUploadTimer;

	UFUNCTION()
	void OnRep_LastUploadedDocHash();

	UFUNCTION()
	void OnRep_LastUploadTime();

	UFUNCTION()
	void OnAutoUploadTimer();

	void OnDownloadDocumentSuccess(const FModumateDocumentHeader& DocHeader, const FMOIDocumentRecord& DocRecord, bool bEmpty);
	void OnDownloadDocumentFailure(int32 ErrorCode, const FString& ErrorMessage);
	void OnUploadedDocument(bool bSuccess, uint32 UploadedDocHash);
	void OnReceivedProjectInfo(const FProjectInfoResponse& InProjectInfo);
};
