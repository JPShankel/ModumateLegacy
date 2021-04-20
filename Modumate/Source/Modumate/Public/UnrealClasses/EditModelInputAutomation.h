// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Queue.h"
#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "Framework/Commands/InputChord.h"
#include "Input/Events.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateCloudConnection.h"

#include "EditModelInputAutomation.generated.h"


MODUMATE_API DECLARE_LOG_CATEGORY_EXTERN(LogInputAutomation, Log, All);

UENUM()
enum class EInputPacketType : uint8
{
	None,
	FrameState,
	Input,
	Command,
	Deltas
};

UENUM()
enum class EInputActionType : uint8
{
	None,
	Key,
	Char,
	MouseButton,
	MouseWheel,
	MouseMove
};

USTRUCT()
struct MODUMATE_API FEditModelInputPacket
{
	GENERATED_BODY()

	UPROPERTY()
	float TimeSeconds = 0.0f;

	UPROPERTY()
	int32 TimeFrame = 0;

	UPROPERTY()
	EInputPacketType Type = EInputPacketType::None;

	UPROPERTY()
	EInputActionType ActionType = EInputActionType::None;

	UPROPERTY()
	FKey InputKey;

	UPROPERTY()
	FString InputChar;

	UPROPERTY()
	float ScrollDelta = 0.0f;

	UPROPERTY()
	TEnumAsByte<EInputEvent> InputEvent = IE_MAX;

	UPROPERTY()
	FTransform CameraTransform;

	UPROPERTY()
	FVector2D MouseScreenPos = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D MouseDelta = FVector2D::ZeroVector;

	UPROPERTY()
	FIntPoint NewViewportSize = FIntPoint::NoneValue;

	UPROPERTY()
	bool bCursorVisible = true;

	UPROPERTY()
	bool bMouseInWorld = false;

	UPROPERTY()
	FString CommandString;

	UPROPERTY()
	FDeltasRecord Deltas;

	bool CompareFrameState(const FEditModelInputPacket &Other) const;
};

USTRUCT()
struct MODUMATE_API FLoggedCloudRequest
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Index = INDEX_NONE;

	UPROPERTY()
	FString URL;

	UPROPERTY()
	float RequestTimeSeconds = 0.0f;

	UPROPERTY()
	int32 RequestTimeFrame = 0;

	UPROPERTY()
	bool bConnectionSuccess = false;

	UPROPERTY()
	int32 ResponseCode = 0;

	UPROPERTY()
	FString ResponseContent;

	UPROPERTY()
	float ResponseTimeSeconds = 0.0f;

	UPROPERTY()
	int32 ResponseTimeFrame = 0;
};

USTRUCT()
struct MODUMATE_API FEditModelInputLog
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ChunkIndex = 0;

	UPROPERTY()
	FIntPoint ViewportSize;

	UPROPERTY()
	FTransform StartCameraTransform;

	UPROPERTY()
	float RecordStartTime;

	UPROPERTY()
	TArray<FEditModelInputPacket> InputPackets;

	UPROPERTY()
	TMap<int32, FLoggedCloudRequest> CloudRequests;

	UPROPERTY()
	bool bRecordedUserData = false;

	UPROPERTY()
	FModumateUserInfo UserInfo;

	UPROPERTY()
	FModumateUserStatus UserStatus;

	UPROPERTY()
	bool bTutorialProject;

	UPROPERTY()
	FName LoadedWalkthroughCategoryName;

	UPROPERTY()
	bool bLoadedDocument = false;

	UPROPERTY()
	FString LoadedDocPath;

	UPROPERTY()
	FModumateDocumentHeaderV2 LoadedDocHeader;
	
	UPROPERTY()
	FMOIDocumentRecordV4 LoadedDocRecord;

	UPROPERTY()
	float RecordEndTime;

	const static uint32 CurInputLogVersion;
	const static FGuid LogFileFooter;
	const static FString LogExtension;

	FEditModelInputLog() { Reset(0.0f); }
	void Reset(float CurTimeSeconds);
};

UENUM()
enum class EInputAutomationState : uint8
{
	None,
	Recording,
	Playing
};

UCLASS(Config = Game)
class MODUMATE_API UEditModelInputAutomation : public UActorComponent, public ICloudConnectionAutomation
{
	GENERATED_BODY()

public:
	UEditModelInputAutomation(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	// Input pre-processor handling functions
	bool PreProcessKeyCharEvent(const FCharacterEvent& InCharEvent);
	bool PreProcessKeyDownEvent(const FKeyEvent& InKeyEvent);
	bool PreProcessKeyUpEvent(const FKeyEvent& InKeyEvent);
	bool PreProcessMouseMoveEvent(const FPointerEvent& MouseEvent);
	bool PreProcessMouseButtonDownEvent(const FPointerEvent& MouseEvent);
	bool PreProcessMouseButtonUpEvent(const FPointerEvent& MouseEvent);
	bool PreProcessMouseWheelEvent(const FPointerEvent& InWheelEvent);

	UFUNCTION(BlueprintPure)
	EInputAutomationState GetCurrentState() const { return CurState; }

	UFUNCTION()
	const FEditModelInputPacket &CurInputPacket() const { return CurInputLogData.InputPackets[CurPacketIndex]; }

	UFUNCTION()
	bool BeginRecording();

	UFUNCTION()
	void TryBeginRecording();

	UFUNCTION(BlueprintPure)
	bool IsRecording() const { return CurState == EInputAutomationState::Recording; }

	void RecordLoadedProject(const FString& LoadedDocPath, const FModumateDocumentHeader& LoadedDocHeader, const FMOIDocumentRecord& LoadedDocRecord);
	void RecordLoadedTutorial(bool bLoadedTutorialProject);
	void RecordLoadedWalkthrough(FName LoadedWalkthroughCategoryName);
	void RecordUserData(const FModumateUserInfo& UserInfo, const FModumateUserStatus& UserStatus);
	void RecordCommand(const FString &CommandString);
	void RecordButton(const FKey& InputKey, EInputEvent InputEvent);
	void RecordCharInput(const TCHAR InputChar, bool bIsRepeat);
	void RecordWheelScroll(float ScrollDelta);
	void RecordMouseMove(const FVector2D& CurPos, const FVector2D& Delta);
	void RecordDeltas(const TArray<FDeltaPtr>& Deltas);

	bool VerifyAppliedDeltas(const TArray<FDeltaPtr>& Deltas);
	bool ApplyDeltaRecord(const FDeltasRecord& DeltasRecord);

	bool PostApplyUserDeltas(const TArray<FDeltaPtr>& Deltas);

	TFunction<bool()> MakeSaveLogTask(const FString& InputLogPath) const;

	UFUNCTION()
	bool EndRecording(bool bPromptForPath);

	UFUNCTION()
	void TryEndRecording();

	UFUNCTION()
	bool BeginPlaybackPrompt(bool bCaptureFrames, float InPlaybackSpeed = 1.0f, bool bExitOnFrameCaptured = false);

	UFUNCTION()
	bool BeginPlayback(const FString& InputLogPath, bool bCaptureFrames, float InPlaybackSpeed = 1.0f, bool bExitOnFrameCaptured = false);

	UFUNCTION()
	void TryBeginPlaybackPrompt();

	UFUNCTION(BlueprintPure)
	bool IsPlaying() const { return CurState == EInputAutomationState::Playing; }

	UFUNCTION()
	bool EndPlayback();

	UFUNCTION()
	void TryEndPlayback();

	UFUNCTION()
	bool ResizeWindowForViewportSize(int32 Width, int32 Height);

	UFUNCTION()
	bool IsPlayingRecordedDeltas() const { return IsPlaying() && bShouldPlayRecordedDeltas; }

	UFUNCTION()
	bool StartPlayingRecordedDeltas();

	UFUNCTION()
	bool ShouldDocumentSkipDeltas() const;

	UFUNCTION()
	bool ShouldDocumentVerifyDeltas() const;

	const FString &GetLastLogPath() const { return LastLogPath; }

	// ICloudConnectionAutomation interface
	virtual bool RecordRequest(FHttpRequestRef Request, int32 RequestIdx) override;
	virtual bool RecordResponse(FHttpRequestRef Request, int32 RequestIdx, bool bConnectionSuccess, int32 ResponseCode, const FString& ResponseContent) override;
	virtual bool GetResponse(FHttpRequestRef Request, int32 RequestIdx, bool& bOutSuccess, int32& OutCode, FString& OutContent, float& OutResponseTime) override;
	virtual FTimerManager& GetTimerManager() const override;

protected:
	TSharedPtr<class FAutomationCaptureInputProcessor> InputProcessor;
	EInputAutomationState CurState;
	float CurAutomationTime;
	int32 CurAutomationFrame;
	int32 CurPacketIndex;
	FEditModelInputLog CurInputLogData;
	bool bCapturingFrames;
	int32 FrameCaptureVideoIndex;
	float FrameCaptureStartTime;
	float PlaybackSpeed;
	bool bWillExitOnFrameCaptured;
	FTimerHandle FrameCaptureSaveTimer;
	FString LastLogPath;
	FString FrameCapturePath;
	class FSceneViewport* SceneViewport;

	bool bShouldPlayRecordedDeltas;
	bool bApplyingRecordedDeltas;
	bool bVerifyingDeltas;
	TQueue<FDeltasRecord> RecordedDeltasToVerify;

	UPROPERTY()
	class AEditModelPlayerController *EMPlayerController;

	UPROPERTY()
	class UGameViewportClient* GameViewport;

	UFUNCTION()
	void CheckFrameCaptureSaved();

	bool FindViewport();
	FEditModelInputPacket &AddRecordingPacket(EInputPacketType Type);
	bool PlayBackPacket(const FEditModelInputPacket &InputPacket, float DeltaTime);
	bool SimulateInput(const FEditModelInputPacket& InputPacket);
	FString GetDefaultInputLogPath(const FString &Extension);
	void StartRecordingFrames();
	void SaveRecordedFrames();

	static bool LoadInputLog(const FString& InputLogPath, FEditModelInputLog& OutLogData);
	static bool SaveInputLog(const FString& InputLogPath, const FEditModelInputLog& LogData);
};
