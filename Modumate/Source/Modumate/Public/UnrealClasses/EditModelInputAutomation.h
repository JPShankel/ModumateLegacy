// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Framework/Commands/InputChord.h"

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
	FKey InputKey;

	UPROPERTY()
	TEnumAsByte<EInputEvent> InputEvent = IE_MAX;

	UPROPERTY()
	FTransform CameraTransform;

	UPROPERTY()
	FVector2D MouseScreenPos = FVector2D::ZeroVector;

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
	FDateTime RecordStartTime;

	UPROPERTY()
	TArray<FEditModelInputPacket> InputPackets;

	UPROPERTY()
	FDateTime RecordEndTime;

	const static uint32 CurInputLogVersion;
	const static FGuid LogFileFooter;
	const static FString LogExtension;

	FEditModelInputLog() { Reset(); }
	void Reset();
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
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	UPROPERTY()
	float FrameCaptureDuration;

	UFUNCTION(BlueprintPure)
	EInputAutomationState GetCurrentState() const { return CurState; }

	UFUNCTION()
	const FEditModelInputPacket &CurInputPacket() const { return CurInputLogData.InputPackets[CurPacketIndex]; }

	UFUNCTION()
	bool BeginRecording();

	UFUNCTION(BlueprintPure)
	bool IsRecording() const { return CurState == EInputAutomationState::Recording; }

	UFUNCTION()
	void RecordCommand(const FString &CommandString);

	UFUNCTION()
	void RecordInput(const FKey &InputKey, EInputEvent InputEvent);

	UFUNCTION()
	bool EndRecording();

	UFUNCTION()
	bool BeginPlaybackPrompt(bool bCaptureFrames, float InPlaybackSpeed = 1.0f);

	UFUNCTION()
	bool BeginPlayback(const FString& InputLogPath, bool bCaptureFrames, float InPlaybackSpeed = 1.0f);

	UFUNCTION(BlueprintPure)
	bool IsPlaying() const { return CurState == EInputAutomationState::Playing; }

	UFUNCTION()
	bool EndPlayback();

	UFUNCTION()
	bool ResizeWindowForViewportSize(int32 Width, int32 Height);

	const FString &GetLastLogPath() const { return LastLogPath; }

protected:
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

	UPROPERTY()
	class AEditModelPlayerController_CPP *EMPlayerController;

	FEditModelInputPacket &AddRecordingPacket(EInputPacketType Type);
	bool PlayBackPacket(const FEditModelInputPacket &InputPacket, float DeltaTime);
	FString GetDefaultInputLogPath(const FString &Extension);
	bool SaveInputLog(const FString& InputLogPath);
	bool LoadInputLog(const FString& InputLogPath);
	void CaptureFrame();
};
