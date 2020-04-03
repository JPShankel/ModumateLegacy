// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelInputAutomation.h"

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerPawn_CPP.h"
#include "HAL/FileManager.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "ModumateConsoleCommand.h"
#include "PlatformFunctions.h"
#include "Slate/SceneViewport.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"
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

const uint32 FEditModelInputLog::CurInputLogVersion = 1;
const FString FEditModelInputLog::LogExtension(TEXT(".ilog"));
const FGuid FEditModelInputLog::LogFileFooter(0x8958689A, 0x9f624172, 0xAA5D7B46, 0xA834850C);

void FEditModelInputLog::Reset()
{
	ViewportSize = FIntPoint::ZeroValue;
	StartCameraTransform.SetIdentity();
	RecordStartTime = FDateTime::Now();
	InputPackets.Reset();
	RecordEndTime = RecordStartTime;
}

UEditModelInputAutomation::UEditModelInputAutomation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FrameCaptureDuration(1.0f / 30.0f)
	, CurState(EInputAutomationState::None)
	, CurAutomationTime(0.0f)
	, CurAutomationFrame(0)
	, CurPacketIndex(0)
	, LastFrameCaptureTime(0.0f)
	, FrameCaptureIndex(0)
	, bCapturingFrames(false)
	, PlaybackSpeed(1.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEditModelInputAutomation::BeginPlay()
{
	Super::BeginPlay();

	EMPlayerController = Cast<AEditModelPlayerController_CPP>(GetOwner());
	ensureAlways(EMPlayerController != nullptr);
}

void UEditModelInputAutomation::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurState)
	{
	case EInputAutomationState::Recording:
	{
		AddRecordingPacket(EInputPacketType::FrameState);
		CurAutomationTime += DeltaTime;
		CurAutomationFrame++;
	}
	break;
	case EInputAutomationState::Playing:
	{
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

		if (bCapturingFrames && ((CurAutomationTime - LastFrameCaptureTime) > FrameCaptureDuration))
		{
			CaptureFrame();
		}

		if (CurPacketIndex >= totalNumPackets)
		{
			EndPlayback();
		}
	}
	break;
	}
}

bool UEditModelInputAutomation::BeginRecording()
{
	if (!ensureAlways(CurState == EInputAutomationState::None))
	{
		return false;
	}

	CurState = EInputAutomationState::Recording;
	CurAutomationTime = 0.0f;
	CurAutomationFrame = 0;
	CurPacketIndex = 0;
	CurInputLogData.Reset();
	EMPlayerController->GetViewportSize(CurInputLogData.ViewportSize.X, CurInputLogData.ViewportSize.Y);
	CurInputLogData.StartCameraTransform = EMPlayerController->CurrentCamera->GetComponentTransform();

	UE_LOG(LogInputAutomation, Log, TEXT("Started recording!"));

	PrimaryComponentTick.SetTickFunctionEnable(true);

	return true;
}

void UEditModelInputAutomation::RecordCommand(const FString &CommandString)
{
	FEditModelInputPacket &curPacket = AddRecordingPacket(EInputPacketType::Command);
	curPacket.CommandString = CommandString;
}

void UEditModelInputAutomation::RecordInput(const FKey &InputKey, EInputEvent InputEvent)
{
	FEditModelInputPacket &curPacket = AddRecordingPacket(EInputPacketType::Input);
	curPacket.InputKey = InputKey;
	curPacket.InputEvent = InputEvent;
}

bool UEditModelInputAutomation::EndRecording()
{
	if (!ensureAlways(CurState == EInputAutomationState::Recording))
	{
		return false;
	}

	CurInputLogData.RecordEndTime = FDateTime::Now();

	FString newLogPath = GetDefaultInputLogPath(FEditModelInputLog::LogExtension);
	bool bSaveSuccess = SaveInputLog(newLogPath);
	if (bSaveSuccess)
	{
		LastLogPath = newLogPath;
	}

	UE_LOG(LogInputAutomation, Log, TEXT("Finished recording!"));

	CurState = EInputAutomationState::None;
	PrimaryComponentTick.SetTickFunctionEnable(false);
	return bSaveSuccess;
}

bool UEditModelInputAutomation::BeginPlaybackPrompt(bool bCaptureFrames, float InPlaybackSpeed)
{
	FString inputLogPath;
	if (Modumate::PlatformFunctions::GetOpenFilename(inputLogPath, false))
	{
		return BeginPlayback(inputLogPath, bCaptureFrames, InPlaybackSpeed);
	}

	return false;
}

bool UEditModelInputAutomation::BeginPlayback(const FString& InputLogPath, bool bCaptureFrames, float InPlaybackSpeed)
{
	if (!ensureAlways(CurState == EInputAutomationState::None) ||
		!LoadInputLog(InputLogPath))
	{
		return false;
	}

	UE_LOG(LogInputAutomation, Log, TEXT("Starting playback from file: %s"), *FPaths::GetCleanFilename(InputLogPath));

	ResizeWindowForViewportSize(CurInputLogData.ViewportSize.X, CurInputLogData.ViewportSize.Y);
	EMPlayerController->CurrentCamera->SetComponentToWorld(CurInputLogData.StartCameraTransform);

	CurState = EInputAutomationState::Playing;
	CurAutomationTime = 0.0f;
	CurAutomationFrame = 0;
	CurPacketIndex = 0;
	LastFrameCaptureTime = 0.0f;
	FrameCaptureIndex = 0;
	bCapturingFrames = bCaptureFrames;
	PlaybackSpeed = InPlaybackSpeed;
	LastLogPath = InputLogPath;
	PrimaryComponentTick.SetTickFunctionEnable(true);

	return true;
}

bool UEditModelInputAutomation::EndPlayback()
{
	if (!ensureAlways(CurState == EInputAutomationState::Playing))
	{
		return false;
	}

	UE_LOG(LogInputAutomation, Log, TEXT("Ending playback at %.2fs."), CurAutomationTime);

	CurState = EInputAutomationState::None;
	PrimaryComponentTick.SetTickFunctionEnable(false);

	return true;
}

bool UEditModelInputAutomation::ResizeWindowForViewportSize(int32 Width, int32 Height)
{
	// Try to find the scene viewport, whether this is inside the editor or not.
	FSceneViewport *sceneViewport = nullptr;

	if (!GIsEditor)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		sceneViewport = GameEngine->SceneViewport.Get();
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
				FSlatePlayInEditorInfo *slatePlayInEditorSessionPtr = editorEngine->SlatePlayInEditorMap.Find(context.ContextHandle);
				if (slatePlayInEditorSessionPtr && slatePlayInEditorSessionPtr->SlatePlayInEditorWindowViewport.IsValid())
				{
					sceneViewport = slatePlayInEditorSessionPtr->SlatePlayInEditorWindowViewport.Get();
				}
			}
		}
	}
#endif

	if (sceneViewport)
	{
		TSharedPtr<SWindow> window = sceneViewport->FindWindow();
		if (window.IsValid())
		{
			// Try to set the window size to match the originally-recorded size.
			FVector2D viewportSize(Width, Height);
			FVector2D newWindowSize = window->GetWindowSizeFromClientSize(viewportSize);

			sceneViewport->ResizeFrame(static_cast<uint32>(newWindowSize.X), static_cast<uint32>(newWindowSize.Y), EWindowMode::Windowed);
		}
	}

	return true;
}


FEditModelInputPacket &UEditModelInputAutomation::AddRecordingPacket(EInputPacketType Type)
{
	CurPacketIndex = CurInputLogData.InputPackets.Num();
	FEditModelInputPacket &newPacket = CurInputLogData.InputPackets.AddDefaulted_GetRef();

	newPacket.Type = Type;
	newPacket.TimeSeconds = CurAutomationTime;
	newPacket.TimeFrame = CurAutomationFrame;

	if (Type == EInputPacketType::FrameState)
	{
		// Populate the packet with the current frame state values we can retrieve from here.
		newPacket.CameraTransform = EMPlayerController->CurrentCamera->GetComponentTransform();
		EMPlayerController->GetMousePosition(newPacket.MouseScreenPos.X, newPacket.MouseScreenPos.Y);

		// Keep track of whether the mouse cursor is visible.
		newPacket.bCursorVisible = EMPlayerController->bShowMouseCursor;

		// Keep track of whether the mouse is over one of our UI widgets.
		newPacket.bMouseInWorld = !EMPlayerController->IsCursorOverWidget();

		// If the frame state is the same as the previous frame, then don't bother to make a new packet
		if (CurInputLogData.InputPackets.Num() >= 2)
		{
			FEditModelInputPacket &prevPacket = CurInputLogData.InputPackets.Last(1);
			if (prevPacket.CompareFrameState(newPacket))
			{
				CurInputLogData.InputPackets.RemoveAt(CurInputLogData.InputPackets.Num() - 1);
				return prevPacket;
			}
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
		EMPlayerController->CurrentCamera->SetComponentToWorld(InputPacket.CameraTransform);
		EMPlayerController->SetMouseLocation(InputPacket.MouseScreenPos.X, InputPacket.MouseScreenPos.Y);
		EMPlayerController->bShowMouseCursor = InputPacket.bCursorVisible;

		return true;
	}
	case EInputPacketType::Input:
	{
		// Play back input, if some was recorded
		if (!InputPacket.InputKey.IsValid() || (InputPacket.InputEvent == IE_MAX))
		{
			return false;
		}

		float amountDepressed = (InputPacket.InputEvent == EInputEvent::IE_Released) ? 0.0f : 1.0f;
		EMPlayerController->InputKey(InputPacket.InputKey, InputPacket.InputEvent, amountDepressed, false);

		return true;
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

FString UEditModelInputAutomation::GetDefaultInputLogPath(const FString &Extension)
{
	FString fileName = FString::Printf(TEXT("InputLog_%s%s"), *FDateTime::Now().ToString(), *Extension);
	static const FString folderName(TEXT("InputAutomation"));
	return FPaths::Combine(FPaths::ProjectSavedDir(), folderName, fileName);
}

bool UEditModelInputAutomation::SaveInputLog(const FString& InputLogPath)
{
	if (InputLogPath.EndsWith(FEditModelInputLog::LogExtension))
	{
		// Create a file writer for this input log
		FArchive *archive = IFileManager::Get().CreateFileWriter(*InputLogPath);
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

		FStructSerializer::Serialize(CurInputLogData, serializerBackend, policies);

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

bool UEditModelInputAutomation::LoadInputLog(const FString& InputLogPath)
{
	TArray<uint8> buffer, compressedBuffer;

	if (InputLogPath.EndsWith(FEditModelInputLog::LogExtension))
	{
		auto archive = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*InputLogPath));
		if (archive == nullptr)
		{
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
				return false;
			}
		}

		// Read the version
		uint32 savedVersion = 0;
		archive->SerializeIntPacked(savedVersion);
		if (savedVersion != FEditModelInputLog::CurInputLogVersion)
		{
			return false;
		}

		// Read the compressed input log
		uint32 uncompressedSize = 0;
		archive->SerializeIntPacked(uncompressedSize);

		if (uncompressedSize == 0)
		{
			return false;
		}

		buffer.AddZeroed(uncompressedSize);
		archive->SerializeCompressed(buffer.GetData(), uncompressedSize, NAME_Zlib, COMPRESS_BiasMemory, true);

		// Deserialize the input log struct
		FMemoryReader reader(buffer);
		FCborStructDeserializerBackend deserializerBackend(reader);

		FStructDeserializerPolicies policies;
		policies.MissingFields = EStructDeserializerErrorPolicies::Ignore;

		CurInputLogData.Reset();
		return FStructDeserializer::Deserialize(CurInputLogData, deserializerBackend, policies);
	}

	return false;
}

void UEditModelInputAutomation::CaptureFrame()
{
	FString screenshotFilename = FString::Printf(TEXT("InputPlaybackFrame%05i.png"), FrameCaptureIndex);
	bool bShowUI = true;
	bool bAddFilenameSuffix = false;
	FScreenshotRequest::RequestScreenshot(screenshotFilename, bShowUI, bAddFilenameSuffix);

	LastFrameCaptureTime = CurAutomationTime;
	++FrameCaptureIndex;
}
