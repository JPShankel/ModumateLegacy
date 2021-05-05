// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelInputAutomation.h"

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "DocumentManagement/ModumateDocument.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetStringLibrary.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Slate/SceneViewport.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"
#include "UI/TutorialManager.h"
#include "UnrealClasses/AutomationCaptureInputProcessor.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#endif


DEFINE_LOG_CATEGORY(LogInputAutomation);

bool FEditModelInputPacket::CompareFrameState(const FEditModelInputPacket &Other) const
{
	return (CameraTransform.Equals(Other.CameraTransform)) &&
		(MouseScreenPos == Other.MouseScreenPos) &&
		(NewViewportSize == Other.NewViewportSize) &&
		(bCursorVisible == Other.bCursorVisible) &&
		(bMouseInWorld == Other.bMouseInWorld);
}

// Version 3: add deltas as a fallback if input desyncs, viewport resizing, tutorial data, and previous projects
const uint32 FEditModelInputLog::CurInputLogVersion = 3;
const FString FEditModelInputLog::LogExtension(TEXT(".ilog"));
const FGuid FEditModelInputLog::LogFileFooter(0x8958689A, 0x9f624172, 0xAA5D7B46, 0xA834850C);

void FEditModelInputLog::Reset(float CurTimeSeconds)
{
	ViewportSize = FIntPoint::NoneValue;
	StartCameraTransform.SetIdentity();
	RecordStartTime = CurTimeSeconds;
	InputPackets.Reset();
	CloudRequests.Reset();
	bRecordedUserData = false;
	UserInfo = FModumateUserInfo();
	UserStatus = FModumateUserStatus();
	LoadedWalkthroughCategoryName = NAME_None;
	bTutorialProject = false;
	bLoadedDocument = false;
	LoadedDocPath.Empty();
	LoadedDocHeader = FModumateDocumentHeader();
	LoadedDocRecord = FMOIDocumentRecord();
	RecordEndTime = RecordStartTime;
}

UEditModelInputAutomation::UEditModelInputAutomation()
	: Super()
	, CurState(EInputAutomationState::None)
	, CurAutomationTime(0.0f)
	, CurAutomationFrame(0)
	, CurPacketIndex(0)
	, bCapturingFrames(false)
	, FrameCaptureVideoIndex(INDEX_NONE)
	, FrameCaptureStartTime(0.0f)
	, PlaybackSpeed(1.0f)
	, bWillExitOnFrameCaptured(false)
	, SceneViewport(nullptr)
	, bShouldPlayRecordedDeltas(false)
	, bApplyingRecordedDeltas(false)
	, bVerifyingDeltas(false)
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
		float nextAutomationTime = CurAutomationTime;
		float nextAutomationFrame = CurAutomationFrame;
		int32 totalNumPackets = CurInputLogData.InputPackets.Num();

		// Allow waiting some time before starting the playback for real.
		if (CurAutomationTime < 0.0f)
		{
			FString debugMessage = FString::Printf(TEXT("Playing input in %d..."), FMath::CeilToInt(-CurAutomationTime));
			GEngine->AddOnScreenDebugMessage(0, 0.0f, FColor::Green, debugMessage);

			nextAutomationTime += DeltaTime;
		}
		else
		{
			float totalLogDuration = (CurInputLogData.RecordEndTime - CurInputLogData.RecordStartTime);
			int32 currentLogPCT = FMath::CeilToInt(100.0 * CurAutomationTime / totalLogDuration);

			FString curLogFilename = FPaths::GetBaseFilename(LastLogPath, true);
			FString debugMessage = FString::Printf(TEXT("\u25B6 Playing input (%s) - %s - %d%%"), *curLogFilename, *automationTimeStr, currentLogPCT);
			GEngine->AddOnScreenDebugMessage(0, 0.0f, FColor::Green, debugMessage);

			nextAutomationTime = CurAutomationTime + (PlaybackSpeed * DeltaTime);
			nextAutomationFrame = CurAutomationFrame + static_cast<int32>(PlaybackSpeed);

			// Search for the next packet(s) to use, based on the frame method (fixed vs. variable)
			while (CurPacketIndex < totalNumPackets)
			{
				const FEditModelInputPacket& nextPacket = CurInputLogData.InputPackets[CurPacketIndex];

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
	auto gameInstance = EMPlayerController->GetGameInstance<UModumateGameInstance>();
	if ((CurState != EInputAutomationState::None) || (SceneViewport == nullptr) || (gameInstance == nullptr))
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

	auto cloudConnection = gameInstance->GetCloudConnection();
	if (cloudConnection.IsValid())
	{
		cloudConnection->SetAutomationHandler(this);
	}

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

void UEditModelInputAutomation::RecordLoadedProject(const FString& LoadedDocPath, const FModumateDocumentHeader& LoadedDocHeader, const FMOIDocumentRecord& LoadedDocRecord)
{
	CurInputLogData.bLoadedDocument = true;
	CurInputLogData.LoadedDocPath = LoadedDocPath;
	CurInputLogData.LoadedDocHeader = LoadedDocHeader;
	CurInputLogData.LoadedDocRecord = LoadedDocRecord;
}

void UEditModelInputAutomation::RecordLoadedTutorial(bool bLoadedTutorialProject)
{
	CurInputLogData.bTutorialProject = bLoadedTutorialProject;
}

void UEditModelInputAutomation::RecordLoadedWalkthrough(FName LoadedWalkthroughCategoryName)
{
	CurInputLogData.bTutorialProject = !LoadedWalkthroughCategoryName.IsNone();
	CurInputLogData.LoadedWalkthroughCategoryName = LoadedWalkthroughCategoryName;
}

void UEditModelInputAutomation::RecordUserData(const FModumateUserInfo& UserInfo, const FModumateUserStatus& UserStatus)
{
	CurInputLogData.bRecordedUserData = true;
	CurInputLogData.UserInfo = UserInfo;
	CurInputLogData.UserStatus = UserStatus;
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

void UEditModelInputAutomation::RecordDeltas(const TArray<FDeltaPtr>& Deltas)
{
	if (Deltas.Num() == 0)
	{
		return;
	}

	FEditModelInputPacket& curPacket = AddRecordingPacket(EInputPacketType::Deltas);
	curPacket.Deltas = FDeltasRecord(Deltas);
}

bool UEditModelInputAutomation::VerifyAppliedDeltas(const TArray<FDeltaPtr>& Deltas)
{
	// We shouldn't be verifying deltas unless we're playing back, and not playing deltas from the log directly
	UModumateDocument* document = EMPlayerController ? EMPlayerController->GetDocument() : nullptr;
	int32 numAppliedDeltas = Deltas.Num();
	if (!IsPlaying() || bShouldPlayRecordedDeltas || (numAppliedDeltas == 0) || (document == nullptr))
	{
		return false;
	}

	bool bSuccess = true;
	bVerifyingDeltas = true;

	// If there are deltas from a recorded packet that match these applied deltas, then we're all good.
	FDeltasRecord appliedDeltas(Deltas);
	FDeltasRecord recordedDeltas;
	bool bDeltasVerified = RecordedDeltasToVerify.Dequeue(recordedDeltas) && (recordedDeltas == appliedDeltas);

	// If not, then undo the deltas that have been applied in error.
	if (!bDeltasVerified)
	{
		UE_LOG(LogInputAutomation, Error, TEXT("Applied %d deltas on [frame %d, time %.2f] that didn't match recorded deltas!"),
			numAppliedDeltas, CurAutomationFrame, CurAutomationTime);

		document->Undo(GetWorld());

		// Next, apply the deltas that were recorded but can't be trusted to come from simulated input in the future.
		do
		{
			bool bSuccessfulApplication = ApplyDeltaRecord(recordedDeltas);

			// We can't recover from failing to apply recorded deltas.
			if (!ensure(bSuccessfulApplication))
			{
				bSuccess = false;
				EndPlayback();
				break;
			}

		} while (RecordedDeltasToVerify.Dequeue(recordedDeltas));

		// Finally, enable delta playback since we're desynchronized from input, and reset to a neutral input state
		StartPlayingRecordedDeltas();
	}

	bVerifyingDeltas = false;
	return bSuccess;
}

bool UEditModelInputAutomation::ApplyDeltaRecord(const FDeltasRecord& DeltasRecord)
{
	UModumateDocument* document = EMPlayerController ? EMPlayerController->GetDocument() : nullptr;
	if (document == nullptr)
	{
		return false;
	}

	// Empty records are vacuously successful
	if (DeltasRecord.IsEmpty())
	{
		return true;
	}

	TArray<FDeltaPtr> deltasToApply;
	for (auto& structWrapper : DeltasRecord.DeltaStructWrappers)
	{
		if (auto deltaPtr = structWrapper.CreateStructFromJSON<FDocumentDelta>())
		{
			deltasToApply.Add(MakeShareable(deltaPtr));
		}
	}

	bApplyingRecordedDeltas = true;
	bool bApplicationSuccess = document->ApplyDeltas(deltasToApply, GetWorld());
	bApplyingRecordedDeltas = false;
	return bApplicationSuccess;
}

bool UEditModelInputAutomation::PostApplyUserDeltas(const TArray<FDeltaPtr>& Deltas)
{
	if (IsRecording())
	{
		RecordDeltas(Deltas);
		return true;
	}
	else if (ShouldDocumentVerifyDeltas())
	{
		return VerifyAppliedDeltas(Deltas);
	}

	return false;
}

TFunction<bool()> UEditModelInputAutomation::MakeSaveLogTask(const FString& InputLogPath) const
{
	return [InputLogData = CurInputLogData, InputLogPath]()
	{
		return UEditModelInputAutomation::SaveInputLog(InputLogPath, InputLogData);
	};
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
		if (!FModumatePlatform::GetSaveFilename(newLogPath, FModumatePlatform::INDEX_ILOGFILE))
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
		bEndSuccess = SaveInputLog(newLogPath, CurInputLogData);
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

	auto gameInstance = EMPlayerController->GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (cloudConnection.IsValid())
	{
		cloudConnection->SetAutomationHandler(nullptr);
	}

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
	if (FModumatePlatform::GetOpenFilename(inputLogPath, false))
	{
		return BeginPlayback(inputLogPath, bCaptureFrames, InPlaybackSpeed);
	}

	return false;
}

bool UEditModelInputAutomation::BeginPlayback(const FString& InputLogPath, bool bCaptureFrames, float InPlaybackSpeed, bool bExitOnFrameCaptured)
{
	auto gameInstance = EMPlayerController->GetGameInstance<UModumateGameInstance>();
	UModumateDocument* document = EMPlayerController ? EMPlayerController->GetDocument() : nullptr;
	if ((CurState != EInputAutomationState::None) || !LoadInputLog(InputLogPath, CurInputLogData) || !ensure(document && gameInstance))
	{
		return false;
	}

	UE_LOG(LogInputAutomation, Log, TEXT("Starting playback from file: %s"), *FPaths::GetCleanFilename(InputLogPath));

	if (!ResizeWindowForViewportSize(CurInputLogData.ViewportSize.X, CurInputLogData.ViewportSize.Y))
	{
		return false;
	}

	if (CurInputLogData.bLoadedDocument)
	{
		if (!document->LoadRecord(GetWorld(), CurInputLogData.LoadedDocHeader, CurInputLogData.LoadedDocRecord))
		{
			return false;
		}
	}

	EModumateWalkthroughCategories walkthroughCategory = EModumateWalkthroughCategories::None;
	if (gameInstance->TutorialManager && !CurInputLogData.LoadedWalkthroughCategoryName.IsNone() &&
		FindEnumValueByName(CurInputLogData.LoadedWalkthroughCategoryName, walkthroughCategory))
	{
		gameInstance->TutorialManager->BeginWalkthrough(walkthroughCategory);
	}

	auto cloudConnection = gameInstance->GetCloudConnection();
	if (cloudConnection.IsValid())
	{
		cloudConnection->SetAutomationHandler(this);
	}

	if (CurInputLogData.bRecordedUserData)
	{
		auto accountManager = gameInstance->GetAccountManager();
		if (accountManager.IsValid())
		{
			accountManager->SetUserInfo(CurInputLogData.UserInfo);
			accountManager->ProcessUserStatus(CurInputLogData.UserStatus, false);
		}
	}

	EMPlayerController->EMPlayerPawn->CameraComponent->SetComponentToWorld(CurInputLogData.StartCameraTransform);
	EMPlayerController->CameraController->bUpdateCameraTransform = false;

	// Delay input playback, to account for resources that may still be loading, video capture initialization, etc.
	static constexpr float FrameCaptureDelay = 2.0f;

	CurState = EInputAutomationState::Playing;
	CurAutomationTime = -FrameCaptureDelay;
	CurAutomationFrame = 0;
	CurPacketIndex = 0;
	bCapturingFrames = bCaptureFrames;
	FrameCaptureVideoIndex = INDEX_NONE;
	PlaybackSpeed = InPlaybackSpeed;
	bWillExitOnFrameCaptured = bCapturingFrames && bExitOnFrameCaptured;
	LastLogPath = InputLogPath;
	bShouldPlayRecordedDeltas = false;
	bApplyingRecordedDeltas = false;
	bVerifyingDeltas = false;
	RecordedDeltasToVerify.Empty();
	PrimaryComponentTick.SetTickFunctionEnable(true);
	GEngine->bForceDisableFrameRateSmoothing = true;
	GEngine->bUseFixedFrameRate = true;

	if (bCapturingFrames)
	{
		FrameCaptureVideoIndex = 0;

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
		StartRecordingFrames();
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
		SaveRecordedFrames();

		GameViewport->SetUseSoftwareCursorWidgets(false);
		bCapturingFrames = false;

		if (bWillExitOnFrameCaptured)
		{
			GetWorld()->GetTimerManager().SetTimer(FrameCaptureSaveTimer, this, &UEditModelInputAutomation::CheckFrameCaptureSaved, 1.0f, true);
		}
	}

	auto gameInstance = EMPlayerController->GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (cloudConnection.IsValid())
	{
		cloudConnection->SetAutomationHandler(nullptr);
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

bool UEditModelInputAutomation::StartPlayingRecordedDeltas()
{
	if (IsPlaying() && !bShouldPlayRecordedDeltas)
	{
		bShouldPlayRecordedDeltas = true;
		EMPlayerController->SetToolMode(EToolMode::VE_SELECT);
		EMPlayerController->DeselectAll();
		return true;
	}

	return false;
}

bool UEditModelInputAutomation::ShouldDocumentSkipDeltas() const
{
	return IsPlaying() && bShouldPlayRecordedDeltas && !bApplyingRecordedDeltas;
}

bool UEditModelInputAutomation::ShouldDocumentVerifyDeltas() const
{
	return IsPlaying() && !bShouldPlayRecordedDeltas && !bVerifyingDeltas;
}

bool UEditModelInputAutomation::RecordRequest(FHttpRequestRef Request, int32 RequestIdx)
{
	if (!IsRecording() || !ensure(!CurInputLogData.CloudRequests.Contains(RequestIdx)))
	{
		return false;
	}

	FLoggedCloudRequest& loggedRequest = CurInputLogData.CloudRequests.Add(RequestIdx);
	loggedRequest.Index = RequestIdx;
	loggedRequest.URL = Request->GetURL();
	loggedRequest.RequestTimeSeconds = CurAutomationTime;
	loggedRequest.RequestTimeFrame = CurAutomationFrame;

	return true;
}

bool UEditModelInputAutomation::RecordResponse(FHttpRequestRef Request, int32 RequestIdx, bool bConnectionSuccess, int32 ResponseCode, const FString& ResponseContent)
{
	if (!IsRecording() || !ensure(CurInputLogData.CloudRequests.Contains(RequestIdx)))
	{
		return false;
	}

	FLoggedCloudRequest& loggedRequest = CurInputLogData.CloudRequests[RequestIdx];
	loggedRequest.bConnectionSuccess = bConnectionSuccess;
	loggedRequest.ResponseCode = ResponseCode;
	loggedRequest.ResponseContent = ResponseContent;
	loggedRequest.ResponseTimeSeconds = CurAutomationTime;
	loggedRequest.ResponseTimeFrame = CurAutomationFrame;

	return true;
}

bool UEditModelInputAutomation::GetResponse(FHttpRequestRef Request, int32 RequestIdx, bool& bOutSuccess, int32& OutCode, FString& OutContent, float& OutResponseTime)
{
	if (!IsPlaying() || !CurInputLogData.CloudRequests.Contains(RequestIdx))
	{
		return false;
	}

	FLoggedCloudRequest& loggedRequest = CurInputLogData.CloudRequests[RequestIdx];
	bOutSuccess = loggedRequest.bConnectionSuccess;
	OutCode = loggedRequest.ResponseCode;
	OutContent = loggedRequest.ResponseContent;
	OutResponseTime = FMath::Max(0.0f, loggedRequest.ResponseTimeSeconds - loggedRequest.RequestTimeSeconds) / PlaybackSpeed;

	return true;
}

FTimerManager& UEditModelInputAutomation::GetTimerManager() const
{
	return GetWorld()->GetTimerManager();
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

	int32 overridePacketIndex = INDEX_NONE;
	if (Type == EInputPacketType::Deltas)
	{
		// For consistency, always add Delta packets before other Input packets that happen simultaneously.
		// This allows us to know that while simulating input, recorded deltas will always be queued up before any simulated deltas occur from input.
		for (int32 latestSimultaneousInputIdx = CurPacketIndex - 1; latestSimultaneousInputIdx >= 0; --latestSimultaneousInputIdx)
		{
			auto& testPacket = CurInputLogData.InputPackets[latestSimultaneousInputIdx];
			if ((testPacket.TimeSeconds == CurAutomationTime) &&
				(testPacket.TimeFrame == CurAutomationFrame))
			{
				if (testPacket.Type == EInputPacketType::Input)
				{
					overridePacketIndex = latestSimultaneousInputIdx;
				}
			}
			else
			{
				break;
			}
		}
	}

	FEditModelInputPacket& newPacket = (overridePacketIndex == INDEX_NONE) ?
		CurInputLogData.InputPackets.AddDefaulted_GetRef() :
		CurInputLogData.InputPackets.InsertDefaulted_GetRef(overridePacketIndex);

	newPacket.Type = Type;
	newPacket.TimeSeconds = CurAutomationTime;
	newPacket.TimeFrame = CurAutomationFrame;
	
	// Populate the packet with the current frame state values we can retrieve from here.
	newPacket.CameraTransform = EMPlayerController->EMPlayerPawn->CameraComponent->GetComponentTransform();

	// Keep track of the viewport size
	EMPlayerController->GetViewportSize(newPacket.NewViewportSize.X, newPacket.NewViewportSize.Y);

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

		FIntPoint curViewportSize;
		EMPlayerController->GetViewportSize(curViewportSize.X, curViewportSize.Y);
		if (curViewportSize != InputPacket.NewViewportSize)
		{
			SaveRecordedFrames();

			if (!ResizeWindowForViewportSize(InputPacket.NewViewportSize.X, InputPacket.NewViewportSize.Y))
			{
				return false;
			}

			StartRecordingFrames();
		}

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
	case EInputPacketType::Deltas:
	{
		UModumateDocument* document = EMPlayerController->GetDocument();
		UWorld* world = GetWorld();

		// If we're taking control of delta application, then apply them directly to the document
		if (bShouldPlayRecordedDeltas)
		{
			return ApplyDeltaRecord(InputPacket.Deltas);
		}
		// Otherwise, make sure that the recorded deltas correspond to those that were generated by the document
		else
		{
			RecordedDeltasToVerify.Enqueue(InputPacket.Deltas);
			return true;
		}

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

	// If we're playing back deltas rather than relying on simulated input, then don't bother to play input events that could interact with state
	if (bShouldPlayRecordedDeltas && (InputPacket.ActionType != EInputActionType::MouseMove))
	{
		return true;
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


void UEditModelInputAutomation::StartRecordingFrames()
{
	if (!bCapturingFrames)
	{
		return;
	}

	float MaxRecordingTime = (CurInputLogData.RecordEndTime - CurInputLogData.RecordStartTime) - CurAutomationTime;
	EMPlayerController->ConsoleCommand(FString::Printf(TEXT("HighlightRecorder.Start %.1f"), MaxRecordingTime));
	FrameCaptureStartTime = CurAutomationTime;
}

void UEditModelInputAutomation::SaveRecordedFrames()
{
	if (!bCapturingFrames)
	{
		return;
	}

	FString logPathPart, logFileNamePart, logExtPart;
	FPaths::Split(LastLogPath, logPathPart, logFileNamePart, logExtPart);
	logFileNamePart.RemoveSpacesInline();
	FString frameCaptureFileName = FString::Printf(TEXT("%s-%d.mp4"), *logFileNamePart, FrameCaptureVideoIndex);

	// Delete an existing file if it's been exported before
	FrameCapturePath = FPaths::VideoCaptureDir() / frameCaptureFileName;
	IFileManager::Get().Delete(*FrameCapturePath, false, false, true);

	// Try to save the recording (with a little extra buffer), which will end up in in the Saved/VideoCaptures directory
	static constexpr float frameCaptureExtraSaveDuration = 1.0f;
	float frameCaptureDuration = frameCaptureExtraSaveDuration + CurAutomationTime - FrameCaptureStartTime;
	EMPlayerController->ConsoleCommand(FString::Printf(TEXT("HighlightRecorder.Save %s %.1f"), *frameCaptureFileName, frameCaptureDuration));
	EMPlayerController->ConsoleCommand(FString(TEXT("HighlightRecorder.Stop")));

	FrameCaptureVideoIndex++;
}

bool UEditModelInputAutomation::SaveInputLog(const FString& InputLogPath, const FEditModelInputLog& LogData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateSaveInputLog);

	if (InputLogPath.EndsWith(FEditModelInputLog::LogExtension))
	{
		// Create a file writer for this input log
		FArchive* archive = IFileManager::Get().CreateFileWriter(*InputLogPath);
		if (archive == nullptr)
		{
			return false;
		}

		TArray<uint8> uncompressedBuffer;
		uint32 uncompressedSize = 0;

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateSerializeInputLog);

			// Save the version
			uint32 curLogVersion = FEditModelInputLog::CurInputLogVersion;
			archive->SerializeIntPacked(curLogVersion);

			// Serialize the input log struct
			FMemoryWriter writer(uncompressedBuffer);
			FCborStructSerializerBackend serializerBackend(writer, EStructSerializerBackendFlags::Default);

			FStructSerializerPolicies policies;
			policies.NullValues = EStructSerializerNullValuePolicies::Ignore;

			// Input log save may be asynchronous
			FStructSerializer::Serialize(LogData, serializerBackend, policies);

			// Save the size of the uncompressed input log struct
			uncompressedSize = uncompressedBuffer.Num();
			archive->SerializeIntPacked(uncompressedSize);
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateCompressInputLog);

			// Save the compressed input log struct
			archive->SerializeCompressed(uncompressedBuffer.GetData(), uncompressedSize, NAME_Zlib, COMPRESS_BiasMemory);
		}

		bool bSaveSuccess = false;
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateWriteInputLog);

			// Save the footer, so we know we didn't fail mid-write
			FGuid footerGuid = FEditModelInputLog::LogFileFooter;
			*archive << footerGuid;

			// Save the file
			if (!archive->IsError() && !archive->IsCriticalError())
			{
				archive->Flush();
				bSaveSuccess = archive->Close();
			}

			delete archive;
		}

		return bSaveSuccess;
	}

	return false;
}

bool UEditModelInputAutomation::LoadInputLog(const FString& InputLogPath, FEditModelInputLog& OutLogData)
{
	TArray<uint8> buffer, compressedBuffer;

	if (!InputLogPath.EndsWith(FEditModelInputLog::LogExtension))
	{
		UE_LOG(LogTemp, Error, TEXT("Input log file must end in %s\n"), *FEditModelInputLog::LogExtension);
		return false;
	}

	auto archive = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*InputLogPath));
	if (archive == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot load input log from path:\n%s\n"), *InputLogPath);
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
			UE_LOG(LogTemp, Error, TEXT("Input log file missing footer; did it fail to download?\n%s\n"), *InputLogPath);
			return false;
		}
	}

	// Read the version
	uint32 savedVersion = 0;
	archive->SerializeIntPacked(savedVersion);
	if (savedVersion != FEditModelInputLog::CurInputLogVersion)
	{
		UE_LOG(LogTemp, Error, TEXT("Input log is incorrect version: %d; current version: %d\n%s\n"),
			savedVersion, FEditModelInputLog::CurInputLogVersion , *InputLogPath);
		return false;
	}

	// Read the compressed input log
	uint32 uncompressedSize = 0;
	archive->SerializeIntPacked(uncompressedSize);

	if (uncompressedSize == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Input log file is empty:\n%s\n"), *InputLogPath);
		return false;
	}

	buffer.AddZeroed(uncompressedSize);
	archive->SerializeCompressed(buffer.GetData(), uncompressedSize, NAME_Zlib, COMPRESS_BiasMemory, true);

	// Deserialize the input log struct
	FMemoryReader reader(buffer);
	FCborStructDeserializerBackend deserializerBackend(reader);

	FStructDeserializerPolicies policies;
	policies.MissingFields = EStructDeserializerErrorPolicies::Ignore;

	OutLogData.Reset(0.0f);
	if (!FStructDeserializer::Deserialize(OutLogData, deserializerBackend, policies))
	{
		UE_LOG(LogTemp, Error, TEXT("Input log file failed to deserialize:\n%s\n"), *InputLogPath);
		return false;
	}

	// If end time didn't save, then set it to be based on the packets rather than the overall recording time
	if ((OutLogData.RecordEndTime == 0.0f) && (OutLogData.InputPackets.Num() > 0))
	{
		OutLogData.RecordEndTime = (OutLogData.InputPackets.Last().TimeSeconds - OutLogData.RecordStartTime);
	}

	return true;
}
