// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelInputAutomation.h"

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetStringLibrary.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Slate/SceneViewport.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"
#include "UnrealClasses/AutomationCaptureInputProcessor.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#endif


DEFINE_LOG_CATEGORY(LogInputAutomation);

bool FEditModelInputPacket::CompareFrameState(const FEditModelInputPacket &Other) const
{
	return (MouseScreenPos == Other.MouseScreenPos) &&
		(bCursorVisible == Other.bCursorVisible) &&
		(bMouseInWorld == Other.bMouseInWorld);
}

const uint32 FEditModelInputLog::CurInputLogVersion = 2;
const FString FEditModelInputLog::LogExtension(TEXT(".ilog"));
const FGuid FEditModelInputLog::LogFileFooter(0x8958689A, 0x9f624172, 0xAA5D7B46, 0xA834850C);

void FEditModelInputLog::Reset(float CurTimeSeconds)
{
	ViewportSize = FIntPoint::ZeroValue;
	StartCameraTransform.SetIdentity();
	RecordStartTime = CurTimeSeconds;
	InputPackets.Reset();
	RecordEndTime = RecordStartTime;
}

UEditModelInputAutomation::UEditModelInputAutomation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurState(EInputAutomationState::None)
	, CurAutomationTime(0.0f)
	, CurAutomationFrame(0)
	, CurPacketIndex(0)
	, bCapturingFrames(false)
	, PlaybackSpeed(1.0f)
	, SceneViewport(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEditModelInputAutomation::BeginPlay()
{
	Super::BeginPlay();

	EMPlayerController = Cast<AEditModelPlayerController>(GetOwner());
	ensureAlways(EMPlayerController != nullptr);

	InputProcessor = MakeShared<FAutomationCaptureInputProcessor>(this);
	FindViewport();
}

void UEditModelInputAutomation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UEditModelInputAutomation::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FString automationTimeStr = UKismetStringLibrary::TimeSecondsToString(CurAutomationTime);

	switch (CurState)
	{
	case EInputAutomationState::Recording:
	{
		FString debugMessage = FString::Printf(TEXT("\u25CF RECORDING INPUT - %s - %d packets"), *automationTimeStr, CurPacketIndex);
		GEngine->AddOnScreenDebugMessage(0, 0.0f, FColor::Red, debugMessage);

		AddRecordingPacket(EInputPacketType::FrameState);
		CurAutomationTime += DeltaTime;
		CurAutomationFrame++;
	}
	break;
	case EInputAutomationState::Playing:
	{
		float totalLogDuration = (CurInputLogData.RecordEndTime - CurInputLogData.RecordStartTime);
		int32 currentLogPCT = FMath::CeilToInt(100.0 * CurAutomationTime / totalLogDuration);

		FString curLogFilename = FPaths::GetBaseFilename(LastLogPath, true);
		FString debugMessage = FString::Printf(TEXT("\u25B6 Playing input (%s) - %s - %d%%"), *curLogFilename, *automationTimeStr, currentLogPCT);
		GEngine->AddOnScreenDebugMessage(0, 0.0f, FColor::Green, debugMessage);

		float nextAutomationTime = CurAutomationTime + (PlaybackSpeed * DeltaTime);
		float nextAutomationFrame = CurAutomationFrame + static_cast<int32>(PlaybackSpeed);

		// Search for the next packet(s) to use, based on the frame method (fixed vs. variable)
		int32 totalNumPackets = CurInputLogData.InputPackets.Num();
		while (CurPacketIndex < totalNumPackets)
		{
			const FEditModelInputPacket &nextPacket = CurInputLogData.InputPackets[CurPacketIndex];

			if ((GEngine->bUseFixedFrameRate && (nextPacket.TimeFrame > CurAutomationFrame)) ||
				(!GEngine->bUseFixedFrameRate && (nextPacket.TimeSeconds > CurAutomationTime)))
			{
				break;
			}

			float packetDeltaTime = nextPacket.TimeSeconds - CurAutomationTime;
			PlayBackPacket(nextPacket, packetDeltaTime);

			CurAutomationTime = nextPacket.TimeSeconds;
			CurAutomationFrame = nextPacket.TimeFrame;
			CurPacketIndex++;
		}

		CurAutomationTime = nextAutomationTime;
		CurAutomationFrame = nextAutomationFrame;

		if (CurPacketIndex >= totalNumPackets)
		{
			EndPlayback();
		}
	}
	break;
	}
}

bool UEditModelInputAutomation::PreProcessKeyCharEvent(const FCharacterEvent& InCharEvent)
{
	RecordCharInput(InCharEvent.GetCharacter(), InCharEvent.IsRepeat());
	return false;
}

bool UEditModelInputAutomation::PreProcessKeyDownEvent(const FKeyEvent& InKeyEvent)
{
	RecordButton(InKeyEvent.GetKey(), InKeyEvent.IsRepeat() ? IE_Repeat : IE_Pressed);
	return false;
}

bool UEditModelInputAutomation::PreProcessKeyUpEvent(const FKeyEvent& InKeyEvent)
{
	RecordButton(InKeyEvent.GetKey(), IE_Released);
	return false;
}

bool UEditModelInputAutomation::PreProcessMouseMoveEvent(const FPointerEvent& MouseEvent)
{
	// Convert cursor from desktop space to viewport space, for consistent playback
	FVector2D viewportMousePos = SceneViewport->VirtualDesktopPixelToViewport(MouseEvent.GetScreenSpacePosition().IntPoint());
	FVector2D viewportMouseDelta = SceneViewport->VirtualDesktopPixelToViewport(MouseEvent.GetCursorDelta().IntPoint());

	RecordMouseMove(viewportMousePos, viewportMouseDelta);
	return false;
}

bool UEditModelInputAutomation::PreProcessMouseButtonDownEvent(const FPointerEvent& MouseEvent)
{
	RecordButton(MouseEvent.GetEffectingButton(), IE_Pressed);
	return false;
}

bool UEditModelInputAutomation::PreProcessMouseButtonUpEvent(const FPointerEvent& MouseEvent)
{
	RecordButton(MouseEvent.GetEffectingButton(), IE_Released);
	return false;
}

bool UEditModelInputAutomation::PreProcessMouseWheelEvent(const FPointerEvent& InWheelEvent)
{
	RecordWheelScroll(InWheelEvent.GetWheelDelta());
	return false;
}

bool UEditModelInputAutomation::BeginRecording()
{
	if ((CurState != EInputAutomationState::None) || (SceneViewport == nullptr))
	{
		return false;
	}

	CurState = EInputAutomationState::Recording;
	CurAutomationTime = 0.0f;
	CurAutomationFrame = 0;
	CurPacketIndex = 0;
	CurInputLogData.Reset(GetWorld()->GetTimeSeconds());
	EMPlayerController->GetViewportSize(CurInputLogData.ViewportSize.X, CurInputLogData.ViewportSize.Y);
	CurInputLogData.StartCameraTransform = EMPlayerController->EMPlayerPawn->CameraComponent->GetComponentTransform();
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	GEngine->bForceDisableFrameRateSmoothing = true;
	GEngine->bUseFixedFrameRate = true;

	UE_LOG(LogInputAutomation, Log, TEXT("Started recording!"));

	PrimaryComponentTick.SetTickFunctionEnable(true);

	return true;
}

void UEditModelInputAutomation::TryBeginRecording()
{
	if (!BeginRecording())
	{
		UE_LOG(LogInputAutomation, Error, TEXT("Failed to begin recording input!"));
	}
}

void UEditModelInputAutomation::RecordCommand(const FString &CommandString)
{
	FEditModelInputPacket &curPacket = AddRecordingPacket(EInputPacketType::Command);
	curPacket.CommandString = CommandString;
}

void UEditModelInputAutomation::RecordButton(const FKey &InputKey, EInputEvent InputEvent)
{
	if (!InputKey.IsValid())
	{
		return;
	}

	FEditModelInputPacket &curPacket = AddRecordingPacket(EInputPacketType::Input);
	curPacket.ActionType = InputKey.IsMouseButton() ? EInputActionType::MouseButton : EInputActionType::Key;
	curPacket.InputKey = InputKey;
	curPacket.InputEvent = InputEvent;
}

void UEditModelInputAutomation::RecordCharInput(const TCHAR InputChar, bool bIsRepeat)
{
	FEditModelInputPacket& curPacket = AddRecordingPacket(EInputPacketType::Input);
	curPacket.ActionType = EInputActionType::Char;
	curPacket.InputChar.AppendChar(InputChar);
	curPacket.InputEvent = bIsRepeat ? IE_Repeat : IE_Pressed;
}

void UEditModelInputAutomation::RecordWheelScroll(float ScrollDelta)
{
	FEditModelInputPacket& curPacket = AddRecordingPacket(EInputPacketType::Input);
	curPacket.ActionType = EInputActionType::MouseWheel;
	curPacket.ScrollDelta = ScrollDelta;
}

void UEditModelInputAutomation::RecordMouseMove(const FVector2D& CurPos, const FVector2D& Delta)
{
	FEditModelInputPacket& curPacket = AddRecordingPacket(EInputPacketType::Input);
	curPacket.ActionType = EInputActionType::MouseMove;
	curPacket.MouseScreenPos = CurPos;
	curPacket.MouseDelta = Delta;
}

bool UEditModelInputAutomation::EndRecording(bool bPromptForPath)
{
	if (CurState != EInputAutomationState::Recording)
	{
		return false;
	}

	CurInputLogData.RecordEndTime = GetWorld()->GetTimeSeconds();

	bool bTrySaveFile = true;
	FString newLogPath;
	if (bPromptForPath)
	{
		if (!Modumate::PlatformFunctions::GetSaveFilename(newLogPath, INDEX_ILOGFILE))
		{
			bTrySaveFile = false;
		}
	}
	else
	{
		newLogPath = GetDefaultInputLogPath(FEditModelInputLog::LogExtension);
	}

	bool bEndSuccess = true;
	if (bTrySaveFile)
	{
		bEndSuccess = SaveInputLog(newLogPath);
		if (bEndSuccess)
		{
			LastLogPath = newLogPath;
		}
		else
		{
			UE_LOG(LogInputAutomation, Error, TEXT("Failed to save recording to: %s"), *newLogPath);
		}
	}

	FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	GEngine->bForceDisableFrameRateSmoothing = false;
	GEngine->bUseFixedFrameRate = false;

	UE_LOG(LogInputAutomation, Log, TEXT("Finished recording!"));

	CurState = EInputAutomationState::None;
	PrimaryComponentTick.SetTickFunctionEnable(false);
	return bEndSuccess;
}

void UEditModelInputAutomation::TryEndRecording()
{
	EndRecording(true);
}

bool UEditModelInputAutomation::BeginPlaybackPrompt(bool bCaptureFrames, float InPlaybackSpeed, bool bExitOnFrameCaptured)
{
	FString inputLogPath;
	if (Modumate::PlatformFunctions::GetOpenFilename(inputLogPath, false))
	{
		return BeginPlayback(inputLogPath, bCaptureFrames, InPlaybackSpeed);
	}

	return false;
}

bool UEditModelInputAutomation::BeginPlayback(const FString& InputLogPath, bool bCaptureFrames, float InPlaybackSpeed, bool bExitOnFrameCaptured)
{
	if ((CurState != EInputAutomationState::None) || !LoadInputLog(InputLogPath))
	{
		return false;
	}

	UE_LOG(LogInputAutomation, Log, TEXT("Starting playback from file: %s"), *FPaths::GetCleanFilename(InputLogPath));

	if (!ResizeWindowForViewportSize(CurInputLogData.ViewportSize.X, CurInputLogData.ViewportSize.Y))
	{
		return false;
	}

	EMPlayerController->EMPlayerPawn->CameraComponent->SetComponentToWorld(CurInputLogData.StartCameraTransform);
	EMPlayerController->CameraController->bUpdateCameraTransform = false;

	CurState = EInputAutomationState::Playing;
	CurAutomationTime = 0.0f;
	CurAutomationFrame = 0;
	CurPacketIndex = 0;
	bCapturingFrames = bCaptureFrames;
	PlaybackSpeed = InPlaybackSpeed;
	bWillExitOnFrameCaptured = bCapturingFrames && bExitOnFrameCaptured;
	LastLogPath = InputLogPath;
	PrimaryComponentTick.SetTickFunctionEnable(true);
	GEngine->bForceDisableFrameRateSmoothing = true;
	GEngine->bUseFixedFrameRate = true;

	if (bCapturingFrames)
	{
		// Use a software-based cursor so it renders into the frame capture
		GameViewport->SetUseSoftwareCursorWidgets(true);

		// This option is required for correct frame times (it seems like it should be default?)
		static const FString encoderTimeParam(TEXT("GameplayMediaEncoder.UseAppTime"));
		bool bIsForcedAppTime = FParse::Param(FCommandLine::Get(), *encoderTimeParam);
		if (!bIsForcedAppTime)
		{
			static const FString paramPrefix(TEXT(" -"));
			FCommandLine::Append(*(paramPrefix + encoderTimeParam));
		}

		// Start recording a video of the playback session, up to a maximum duration of the reported session length
		float totalLogDuration = (CurInputLogData.RecordEndTime - CurInputLogData.RecordStartTime);
		EMPlayerController->ConsoleCommand(FString::Printf(TEXT("HighlightRecorder.Start %.1f"), totalLogDuration + 1.0f));
	}

	return true;
}

void UEditModelInputAutomation::TryBeginPlaybackPrompt()
{
	if (!BeginPlaybackPrompt(false))
	{
		UE_LOG(LogInputAutomation, Error, TEXT("Failed to begin input playback!"));
	}
}

bool UEditModelInputAutomation::EndPlayback()
{
	if (!ensureAlways(CurState == EInputAutomationState::Playing))
	{
		return false;
	}

	UE_LOG(LogInputAutomation, Log, TEXT("Ending playback at %.2fs."), CurAutomationTime);

	CurState = EInputAutomationState::None;
	EMPlayerController->CameraController->bUpdateCameraTransform = true;
	PrimaryComponentTick.SetTickFunctionEnable(false);
	GEngine->bForceDisableFrameRateSmoothing = false;
	GEngine->bUseFixedFrameRate = false;

	if (bCapturingFrames)
	{
		const static FString frameCaptureExt(TEXT(".mp4"));

		FString logPathPart, logFileNamePart, logExtPart;
		FPaths::Split(LastLogPath, logPathPart, logFileNamePart, logExtPart);
		logFileNamePart.RemoveSpacesInline();
		FString frameCaptureFileName = logFileNamePart + frameCaptureExt;

		// Delete an existing file if it's been exported before
		FrameCapturePath = FPaths::VideoCaptureDir() / frameCaptureFileName;
		IFileManager::Get().Delete(*FrameCapturePath, false, false, true);

		// Try to save the recording, which will end up in in the Saved/VideoCaptures directory
		EMPlayerController->ConsoleCommand(FString::Printf(TEXT("HighlightRecorder.Save %s %.1f"), *frameCaptureFileName, CurAutomationTime));
		GameViewport->SetUseSoftwareCursorWidgets(false);
		bCapturingFrames = false;

		if (bWillExitOnFrameCaptured)
		{
			GetWorld()->GetTimerManager().SetTimer(FrameCaptureSaveTimer, this, &UEditModelInputAutomation::CheckFrameCaptureSaved, true, 1.0f);
		}
	}

	return true;
}

void UEditModelInputAutomation::TryEndPlayback()
{
	EndPlayback();
}

bool UEditModelInputAutomation::ResizeWindowForViewportSize(int32 Width, int32 Height)
{
	if (SceneViewport == nullptr)
	{
		return false;
	}

	TSharedPtr<SWindow> window = SceneViewport->FindWindow();
	if (window.IsValid())
	{
		// Try to set the window size to match the originally-recorded size.
		FVector2D viewportSize(Width, Height);
		FVector2D newWindowSize = window->GetWindowSizeFromClientSize(viewportSize);

		SceneViewport->ResizeFrame(static_cast<uint32>(newWindowSize.X), static_cast<uint32>(newWindowSize.Y), EWindowMode::Windowed);

		// Force the window to the front, to make sure it gets mouse focus.
		window->HACK_ForceToFront();
		window->GetNativeWindow()->SetWindowFocus();
		window->BringToFront(true);
	}

	return true;
}

void UEditModelInputAutomation::CheckFrameCaptureSaved()
{
	if (!FrameCapturePath.IsEmpty() && FPaths::FileExists(FrameCapturePath) && bWillExitOnFrameCaptured)
	{
		FPlatformMisc::RequestExit(false);
	}
}

bool UEditModelInputAutomation::FindViewport()
{
	// Try to find the scene viewport, whether this is inside the editor or not.
	SceneViewport = nullptr;

	if (!GIsEditor)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		SceneViewport = GameEngine->SceneViewport.Get();
	}
#if WITH_EDITOR
	else
	{
		// Try to find the dedicated play-in-editor window viewport, because that's the only one that can be resized.
		// See UEditorEngine::GetPIEViewport() for reference.
		for (const FWorldContext& context : GEngine->GetWorldContexts())
		{
			if (context.WorldType == EWorldType::PIE)
			{
				UEditorEngine* editorEngine = CastChecked<UEditorEngine>(GEngine);
				FSlatePlayInEditorInfo* slatePlayInEditorSessionPtr = editorEngine->SlatePlayInEditorMap.Find(context.ContextHandle);
				if (slatePlayInEditorSessionPtr && slatePlayInEditorSessionPtr->SlatePlayInEditorWindowViewport.IsValid())
				{
					SceneViewport = slatePlayInEditorSessionPtr->SlatePlayInEditorWindowViewport.Get();
				}
			}
		}
	}
#endif

	UWorld* world = GetWorld();
	GameViewport = world ? world->GetGameViewport() : nullptr;

	return (SceneViewport != nullptr) && (GameViewport != nullptr);
}

FEditModelInputPacket &UEditModelInputAutomation::AddRecordingPacket(EInputPacketType Type)
{
	CurPacketIndex = CurInputLogData.InputPackets.Num();
	FEditModelInputPacket &newPacket = CurInputLogData.InputPackets.AddDefaulted_GetRef();

	newPacket.Type = Type;
	newPacket.TimeSeconds = CurAutomationTime;
	newPacket.TimeFrame = CurAutomationFrame;
	
	// Populate the packet with the current frame state values we can retrieve from here.
	newPacket.CameraTransform = EMPlayerController->EMPlayerPawn->CameraComponent->GetComponentTransform();

	// Keep track of whether the mouse cursor is visible.
	newPacket.bCursorVisible = EMPlayerController->bShowMouseCursor;

	// Keep track of whether the mouse is over one of our UI widgets.
	newPacket.bMouseInWorld = !EMPlayerController->IsCursorOverWidget();

	// If the frame state is the same as the previous frame, then don't bother to make a new packet
	if ((Type == EInputPacketType::FrameState) && (CurInputLogData.InputPackets.Num() >= 2))
	{
		FEditModelInputPacket& prevPacket = CurInputLogData.InputPackets.Last(1);
		if (prevPacket.CompareFrameState(newPacket))
		{
			CurInputLogData.InputPackets.RemoveAt(CurInputLogData.InputPackets.Num() - 1);
			return prevPacket;
		}
	}

	return newPacket;
}

bool UEditModelInputAutomation::PlayBackPacket(const FEditModelInputPacket &InputPacket, float DeltaTime)
{
	switch (InputPacket.Type)
	{
	case EInputPacketType::FrameState:
	{
		// Update state that's captured every frame
		EMPlayerController->EMPlayerPawn->CameraComponent->SetComponentToWorld(InputPacket.CameraTransform);
		return true;
	}
	case EInputPacketType::Input:
	{
		return SimulateInput(InputPacket);
	}
	break;
	case EInputPacketType::Command:
	{
		// Play back a command, if one was recorded
		if (InputPacket.CommandString.IsEmpty())
		{
			return false;
		}

		EMPlayerController->ModumateCommand(Modumate::FModumateCommand::FromJSONString(InputPacket.CommandString));
		return true;
	}
	default:
		return false;
	}
}

bool UEditModelInputAutomation::SimulateInput(const FEditModelInputPacket& InputPacket)
{
	// Play back input, if some was recorded
	auto& key = InputPacket.InputKey;
	bool bIsPress = (InputPacket.InputEvent == EInputEvent::IE_Pressed) || (InputPacket.InputEvent == EInputEvent::IE_Repeat);
	bool bIsRepeat = (InputPacket.InputEvent == EInputEvent::IE_Repeat);
	bool bIsRelease = (InputPacket.InputEvent == EInputEvent::IE_Released);
	auto& slateApp = FSlateApplication::Get();
	auto slateUser = slateApp.GetUser(FSlateApplication::CursorUserIndex);
	if (!slateUser.IsValid())
	{
		return false;
	}

	// Generate fake inputs to Slate at the lowest level, as if they're coming from pumping OS input event messages
	switch (InputPacket.ActionType)
	{
	case EInputActionType::Key:
	{
		if (!key.IsValid())
		{
			return false;
		}

		uint32 keyCode = 0x0, charCode = 0x0;
		const uint32* keyCodePtr = nullptr, * charCodePtr = nullptr;
		FInputKeyManager::Get().GetCodesFromKey(InputPacket.InputKey, keyCodePtr, charCodePtr);
		keyCode = keyCodePtr ? *keyCodePtr : keyCode;
		charCode = charCodePtr ? *charCodePtr : charCode;
		if ((keyCode == 0x0) && (charCode == 0x0))
		{
			return false;
		}

		if (bIsPress)
		{
			return slateApp.OnKeyDown(keyCode, charCode, bIsRepeat);
		}
		else if (bIsRelease)
		{
			return slateApp.OnKeyUp(keyCode, charCode, bIsRepeat);
		}
		else
		{
			return false;
		}
	}
	case EInputActionType::Char:
	{
		if (key.IsValid() || (InputPacket.InputChar.Len() != 1) || !bIsPress)
		{
			return false;
		}

		return slateApp.OnKeyChar(InputPacket.InputChar[0], bIsRepeat);
	}
	case EInputActionType::MouseButton:
	{
		if (!key.IsMouseButton())
		{
			return false;
		}

		FPointerEvent pointerEvent(
			slateUser->GetUserIndex(),
			FSlateApplication::CursorPointerIndex,
			slateUser->GetCursorPosition(),
			slateUser->GetPreviousCursorPosition(),
			slateApp.GetPressedMouseButtons(),
			key,
			0.0f,
			slateApp.GetModifierKeys()
		);

		if (bIsPress)
		{
			TSharedPtr<FGenericWindow> genWindow;
			return slateApp.ProcessMouseButtonDownEvent(genWindow, pointerEvent);
		}
		else if (bIsRelease)
		{
			return slateApp.ProcessMouseButtonUpEvent(pointerEvent);
		}
		else
		{
			return false;
		}
	}
	case EInputActionType::MouseWheel:
	{
		if (key.IsValid() || (InputPacket.ScrollDelta == 0.0f))
		{
			return false;
		}

		return slateApp.OnMouseWheel(InputPacket.ScrollDelta);
	}
	case EInputActionType::MouseMove:
	{
		FVector2D oldMouseLocation = slateUser->GetCursorPosition();

		// Convert cursor from viewport space back to desktop space, for interfacing with Slate
		FVector2D newDesktopMousePos(SceneViewport->ViewportToVirtualDesktopPixel(InputPacket.MouseScreenPos));
		FVector2D desktopMouseDelta(SceneViewport->ViewportToVirtualDesktopPixel(InputPacket.MouseDelta));

		if ((oldMouseLocation == newDesktopMousePos) && desktopMouseDelta.IsZero())
		{
			return true;
		}

		slateUser->SetCursorPosition(newDesktopMousePos);
		FVector2D newClampedLocation = slateUser->GetCursorPosition();

		// Create a fake mouse event
		FPointerEvent pointerEvent(
			slateUser->GetUserIndex(),
			newClampedLocation,
			oldMouseLocation,
			desktopMouseDelta,
			slateApp.GetPressedMouseButtons(),
			slateApp.GetModifierKeys()
		);

		// Process the fake mouse event, as though it were at the OS level, but bypassing cursor equality checks
		slateApp.ProcessMouseMoveEvent(pointerEvent);

		return true;
	}
	case EInputActionType::None:
	default:
		return false;
	}
}

FString UEditModelInputAutomation::GetDefaultInputLogPath(const FString &Extension)
{
	FString fileName = FString::Printf(TEXT("InputLog_%s%s"), *FDateTime::Now().ToString(), *Extension);
	static const FString folderName(TEXT("InputAutomation"));
	return FPaths::Combine(FPaths::ProjectSavedDir(), folderName, fileName);
}

TFunction<bool()> UEditModelInputAutomation::MakeSaveLogTask(const FString& InputLogPath) const
{
	return [InputLogData = CurInputLogData,InputLogPath]()
	{
		UE_LOG(LogTemp, Log, TEXT("Telemetry: Running Input Save Log Task"));
		return UEditModelInputAutomation::DoSaveInputLog(InputLogPath, InputLogData);
	};
}

bool UEditModelInputAutomation::DoSaveInputLog(const FString& InputLogPath, const FEditModelInputLog& LogData)
{
	if (InputLogPath.EndsWith(FEditModelInputLog::LogExtension))
	{
		// Create a file writer for this input log
		FArchive* archive = IFileManager::Get().CreateFileWriter(*InputLogPath);
		if (archive == nullptr)
		{
			return false;
		}

		// Save the version
		uint32 curLogVersion = FEditModelInputLog::CurInputLogVersion;
		archive->SerializeIntPacked(curLogVersion);

		// Serialize the input log struct
		TArray<uint8> buffer, compressedBuffer;
		FMemoryWriter writer(buffer);
		FCborStructSerializerBackend serializerBackend(writer, EStructSerializerBackendFlags::Default);

		FStructSerializerPolicies policies;
		policies.NullValues = EStructSerializerNullValuePolicies::Ignore;

		// Input log save may be asynchronous
		FStructSerializer::Serialize(LogData, serializerBackend, policies);

		// Save the size of the uncompressed input log struct
		uint32 uncompressedSize = buffer.Num();
		archive->SerializeIntPacked(uncompressedSize);

		// Save the compressed input log struct
		archive->SerializeCompressed(buffer.GetData(), uncompressedSize, NAME_Zlib, COMPRESS_BiasMemory);

		// Save the footer, so we know we didn't fail mid-write
		FGuid footerGuid = FEditModelInputLog::LogFileFooter;
		*archive << footerGuid;

		// Save the file
		bool bSaveSuccess = false;
		if (!archive->IsError() && !archive->IsCriticalError())
		{
			archive->Flush();
			bSaveSuccess = archive->Close();
		}

		delete archive;
		return bSaveSuccess;
	}

	return false;
}


bool UEditModelInputAutomation::SaveInputLog(const FString& InputLogPath)
{
	return DoSaveInputLog(InputLogPath, CurInputLogData);
}

bool UEditModelInputAutomation::LoadInputLog(const FString& InputLogPath)
{
	static const TCHAR* errorTitle = TEXT("Input Log Load Failure");
	TArray<uint8> buffer, compressedBuffer;

	if (!InputLogPath.EndsWith(FEditModelInputLog::LogExtension))
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("Input log file must end in %s\n"), *FEditModelInputLog::LogExtension), errorTitle);
		return false;
	}

	auto archive = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*InputLogPath));
	if (archive == nullptr)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("Cannot load input log from path:\n%s\n"), *InputLogPath), errorTitle);
		return false;
	}

	// Test that the footer is in place, so we know we didn't fail mid-write
	const int64 totalSize = archive->TotalSize();
	if (totalSize >= sizeof(FGuid))
	{
		FGuid serializedFooter;
		archive->Seek(totalSize - sizeof(FGuid));
		*archive << serializedFooter;
		archive->Seek(0);
		if (serializedFooter != FEditModelInputLog::LogFileFooter)
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("Input log file missing footer; did it fail to download?\n%s\n"), *InputLogPath), errorTitle);
			return false;
		}
	}

	// Read the version
	uint32 savedVersion = 0;
	archive->SerializeIntPacked(savedVersion);
	if (savedVersion != FEditModelInputLog::CurInputLogVersion)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
			*FString::Printf(TEXT("Input log is incorrect version: %d; current version: %d\n%s\n"),
				savedVersion, FEditModelInputLog::CurInputLogVersion , *InputLogPath),
			errorTitle);
		return false;
	}

	// Read the compressed input log
	uint32 uncompressedSize = 0;
	archive->SerializeIntPacked(uncompressedSize);

	if (uncompressedSize == 0)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("Input log file is empty:\n%s\n"), *InputLogPath), errorTitle);
		return false;
	}

	buffer.AddZeroed(uncompressedSize);
	archive->SerializeCompressed(buffer.GetData(), uncompressedSize, NAME_Zlib, COMPRESS_BiasMemory, true);

	// Deserialize the input log struct
	FMemoryReader reader(buffer);
	FCborStructDeserializerBackend deserializerBackend(reader);

	FStructDeserializerPolicies policies;
	policies.MissingFields = EStructDeserializerErrorPolicies::Ignore;

	CurInputLogData.Reset(0.0f);
	if (!FStructDeserializer::Deserialize(CurInputLogData, deserializerBackend, policies))
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("Input log file failed to deserialize:\n%s\n"), *InputLogPath), errorTitle);
		return false;
	}

	// If end time didn't save, then set it to be based on the packets rather than the overall recording time
	if ((CurInputLogData.RecordEndTime == 0.0f) && (CurInputLogData.InputPackets.Num() > 0))
	{
		CurInputLogData.RecordEndTime = (CurInputLogData.InputPackets.Last().TimeSeconds - CurInputLogData.RecordStartTime);
	}

	return true;
}
