// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Framework/Commands/InputChord.h"
#include "Input/Events.h"

#include "EditModelInputAutomation.generated.h"


MODUMATE_API DECLARE_LOG_CATEGORY_EXTERN(LogInputAutomation, Log, All);

UENUM()
enum class EInputPacketType : uint8
{
	None,
	FrameState,
	Input,
	Command
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
	bool bCursorVisible = true;

	UPROPERTY()
	bool bMouseInWorld = false;

	UPROPERTY()
	FString CommandString;

	bool CompareFrameState(const FEditModelInputPacket &Other) const;
};

USTRUCT()
struct MODUMATE_API FEditModelInputLog
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint ViewportSize;

	UPROPERTY()
	FTransform StartCameraTransform;

	UPROPERTY()
	float RecordStartTime;

	UPROPERTY()
	TArray<FEditModelInputPacket> InputPackets;

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
class MODUMATE_API UEditModelInputAutomation : public UActorComponent
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

	UPROPERTY()
	float FrameCaptureDuration;

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

	void RecordCommand(const FString &CommandString);
	void RecordButton(const FKey& InputKey, EInputEvent InputEvent);
	void RecordCharInput(const TCHAR InputChar, bool bIsRepeat);
	void RecordWheelScroll(float ScrollDelta);
	void RecordMouseMove(const FVector2D& CurPos, const FVector2D& Delta);

	UFUNCTION()
	bool EndRecording(bool bPromptForPath);

	UFUNCTION()
	void TryEndRecording();

	UFUNCTION()
	bool BeginPlaybackPrompt(bool bCaptureFrames, float InPlaybackSpeed = 1.0f);

	UFUNCTION()
	bool BeginPlayback(const FString& InputLogPath, bool bCaptureFrames, float InPlaybackSpeed = 1.0f);

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

	const FString &GetLastLogPath() const { return LastLogPath; }

protected:
	TSharedPtr<class FAutomationCaptureInputProcessor> InputProcessor;
	EInputAutomationState CurState;
	float CurAutomationTime;
	int32 CurAutomationFrame;
	int32 CurPacketIndex;
	FEditModelInputLog CurInputLogData;
	float LastFrameCaptureTime;
	int32 FrameCaptureIndex;
	bool bCapturingFrames;
	float PlaybackSpeed;
	FString LastLogPath;
	class FSceneViewport* SceneViewport;

	UPROPERTY()
	class AEditModelPlayerController *EMPlayerController;

	bool FindViewport();
	FEditModelInputPacket &AddRecordingPacket(EInputPacketType Type);
	bool PlayBackPacket(const FEditModelInputPacket &InputPacket, float DeltaTime);
	bool SimulateInput(const FEditModelInputPacket& InputPacket);
	FString GetDefaultInputLogPath(const FString &Extension);
	bool LoadInputLog(const FString& InputLogPath);
	void CaptureFrame();

	// Need public interface for player controller record
public:
	bool SaveInputLog(const FString& InputLogPath);

};
