// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelInputHandler.h"

#include "Database/ModumateDataTables.h"
#include "DocumentManagement/ModumateCommands.h"
#include "GameFramework/InputSettings.h"
#include "GameMapsSettings.h"
#include "HAL/PlatformMisc.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Objects/ModumateObjectStatics.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ModumateSettingsMenu.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "ModumateCore/EnumHelpers.h"

UEditModelInputHandler::UEditModelInputHandler()
	: Super()
	, SequenceResetTime(1.0f)
{
}

void UEditModelInputHandler::BeginPlay()
{
	Super::BeginPlay();

	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
}

void UEditModelInputHandler::SetupBindings()
{
	Controller = Cast<AEditModelPlayerController>(GetOwner());
	UInputSettings *inputSettings = UInputSettings::GetInputSettings();

	if (ensureAlways(Controller && Controller->InputComponent && inputSettings && InputCommandDataTable))
	{
		BoundAxisKeys.Reset();
		auto& axisMappings = inputSettings->GetAxisMappings();
		for (auto& axisMapping : axisMappings)
		{
			BoundAxisKeys.Add(axisMapping.Key);
		}

		FKey NumberKeys[12] = {
			EKeys::Zero,
			EKeys::One,
			EKeys::Two,
			EKeys::Three,
			EKeys::Four,
			EKeys::Five,
			EKeys::Six,
			EKeys::Seven,
			EKeys::Eight,
			EKeys::Nine,
			EKeys::Hyphen,
			EKeys::Period
		};

		FKey NumpadKeys[12] = {
			EKeys::NumPadZero,
			EKeys::NumPadOne,
			EKeys::NumPadTwo,
			EKeys::NumPadThree,
			EKeys::NumPadFour,
			EKeys::NumPadFive,
			EKeys::NumPadSix,
			EKeys::NumPadSeven,
			EKeys::NumPadEight,
			EKeys::NumPadNine,
			EKeys::Subtract,
			EKeys::Decimal
		};

		int32 keysIdx = 1;
		for (auto& keys : { NumberKeys, NumpadKeys })
		{
			for (int32 i = 0; i < 12; ++i)
			{
				FName chordActionName(*FString::Printf(TEXT("InputDigit_%s%d"), (keysIdx == 1 ? TEXT("number") : TEXT("numpad")), i));
				FInputActionKeyMapping commandMapping(chordActionName, keys[i], false, false, false, false);
				inputSettings->AddActionMapping(commandMapping, true);

				Controller->InputComponent->BindAction<FInputDigitDelegate>(chordActionName, EInputEvent::IE_Pressed, this, &UEditModelInputHandler::HandleDigitKey, i);
			}
			keysIdx++;
		}

		RootCommandTrie = MakeShared<FCommandTrieNode>();
		BoundChords.Reset();

		// First, combine all of the input command rows to the central command data map
		InputCommandDataTable->ForeachRow<FInputCommandDataRow>(FString(TEXT("InputCommandDataTable")),
			[this](const FName &Key, const FInputCommandDataRow &Row)
		{
			if (!ensureAlways(Row.Command != EInputCommand::None))
			{
				return;
			}

			FInputCommandData &data = AllInputCommandData.FindOrAdd(Row.Command);

			data.Command = Row.Command;
			data.ActionName = FName(*GetEnumValueString(data.Command));
			data.Title = data.Title.IsEmpty() ? Row.Title : data.Title;
			data.EnabledDescription = data.EnabledDescription.IsEmpty() ? Row.EnabledDescription : data.EnabledDescription;
			data.DisabledDescription = data.DisabledDescription.IsEmpty() ? Row.DisabledDescription : data.DisabledDescription;

			if (Row.Binding.Num() > 0)
			{
				data.AllBindings.Add(Row.Binding);
				data.NumBindings++;
			}
		});

		// Next, for each command, fill the input chord trie and bind the actions
		for (const auto &kvp : AllInputCommandData)
		{
			const FInputCommandData &data = kvp.Value;

			int32 numBindings = data.AllBindings.Num();
			for (int32 bindingIdx = 0; bindingIdx < numBindings; ++bindingIdx)
			{
				const auto &binding = data.AllBindings[bindingIdx];
				int32 numChords = binding.Num();
				if (numChords == 0)
				{
					continue;
				}

				TSharedPtr<FCommandTrieNode> curTrieNode = RootCommandTrie;

				for (int32 chordIdx = 0; chordIdx < numChords; ++chordIdx)
				{
					FInputChord curChord = binding[chordIdx];

					if (!curTrieNode->Children.Contains(curChord))
					{
						curTrieNode->Children.Add(curChord, MakeShared<FCommandTrieNode>());
					}
					curTrieNode = curTrieNode->Children[curChord];
					if (chordIdx == (numChords - 1))
					{
						curTrieNode->InputCommand = data.Command;
					}

					if (!BoundChords.Contains(curChord))
					{
						FName chordActionName(*FString::Printf(TEXT("InputActionChord_%s"), *curChord.GetInputText().ToString()));
						//FName commandChordActionName(*FString::Printf(TEXT("%s_%d.%d"), *data.ActionName.ToString(), bindingIdx, chordIdx));
						FInputActionKeyMapping commandMapping(chordActionName, curChord.Key, curChord.bShift, curChord.bCtrl, curChord.bAlt, curChord.bCmd);
						inputSettings->AddActionMapping(commandMapping, true);

						Controller->InputComponent->BindAction<FInputChordDelegate>(chordActionName, EInputEvent::IE_Pressed, this, &UEditModelInputHandler::HandleBoundChord, curChord);

						BoundChords.Add(curChord);
					}
				}
			}
		}

		CurCommandNode = RootCommandTrie;
	}
}

bool UEditModelInputHandler::TryCommandInternal(EInputCommand Command)
{
	if (Controller == nullptr)
	{
		return false;
	}

	// Special case non-switch handling for tools, for convenience
	EToolMode commandToolMode = ToolModeFromInputCommand(Command);
	if (commandToolMode != EToolMode::VE_NONE)
	{
		Controller->SetToolMode(commandToolMode);
		return true;
	}

	auto currentTool = Cast<UEditModelToolBase>(Controller->CurrentTool.GetObject());

	switch (Command)
	{
		// General app and I/O commands

	case EInputCommand::New:
	{
		Controller->NewModel(true);
		return true;
	}
	case EInputCommand::Open:
	{
		Controller->LoadModel();
		return true;
	}
	case EInputCommand::Save:
	{
		Controller->SaveModel();
		return true;
	}
	case EInputCommand::SaveAs:
	{
		Controller->SaveModelAs();
		return true;
	}
	case EInputCommand::MainMenu:
	{
		Controller->UploadWebThumbnail();
		if (Controller->CheckSaveModel())
		{
			FString mainMenuMap = UGameMapsSettings::GetGameDefaultMap();
			UGameplayStatics::OpenLevel(this, FName(*mainMenuMap));
		}

		return true;
	}
	case EInputCommand::Settings:
	{
		Controller->EditModelUserWidget->ToggleSettingsWindow(!Controller->EditModelUserWidget->SettingsMenuWidget->IsVisible());
		return true;
	}
	case EInputCommand::Exit:
	{
		Controller->UploadWebThumbnail();
		if (Controller->CheckSaveModel())
		{
			FPlatformMisc::RequestExit(false);
		}
		return true;
	}
	case EInputCommand::Undo:
	{
		Controller->HandleUndo();
		return true;
	}
	case EInputCommand::Redo:
	{
		Controller->HandleRedo();
		return true;
	}
	case EInputCommand::TakeScreenshot:
	{
		return Controller->TakeScreenshot();
	}
	case EInputCommand::EscapeKey:
	{
		Controller->HandleEscapeKey();
		return true;
	}

	// General object commands

	case EInputCommand::Copy:
	{
		Controller->ModumateCommand(FModumateCommand(ModumateCommands::kCopySelected));
		return true;
	}
	case EInputCommand::Cut:
	{
		Controller->ModumateCommand(FModumateCommand(ModumateCommands::kCutSelected));
		return true;
	}
	case EInputCommand::Paste:
	{
		Controller->ModumateCommand(FModumateCommand(ModumateCommands::kPaste));
		return true;
	}
	case EInputCommand::SelectAll:
	{
		Controller->SelectAll();
		return true;
	}
	case EInputCommand::SelectInverse:
	{
		Controller->SelectInverse();
		return true;
	}
	case EInputCommand::Deselect:
	{
		Controller->DeselectAll();
		return true;
	}
	case EInputCommand::DeleteDefault:
	{
		Controller->ModumateCommand(
			FModumateCommand(ModumateCommands::kDeleteSelectedObjects)
			.Param(ModumateParameters::kIncludeConnected, true)
		);
		return true;
	}
	case EInputCommand::DeleteOnlySelected:
	{
		Controller->ModumateCommand(
			FModumateCommand(ModumateCommands::kDeleteSelectedObjects)
			.Param(ModumateParameters::kIncludeConnected, false)
		);
		return true;
	}
	case EInputCommand::HideSelected:
	{
		return Controller->HideSelected();
	}
	case EInputCommand::UnhideAll:
	{
		return Controller->UnhideAll();
	}
	case EInputCommand::ToggleUserSnapPoint:
	{
		Controller->ToggleUserSnapPoint();
		return true;
	}
	case EInputCommand::ZoomExtents:
	{
		return Controller->CameraController->ZoomToProjectExtents(FVector::ZeroVector, FVector::ZeroVector);
	}
	case EInputCommand::ZoomSelected:
	{
		return Controller->CameraController->ZoomToSelection(FVector::ZeroVector, FVector::ZeroVector);
	}
	case EInputCommand::ToggleFullscreen:
	{
		// Ignore the fullscreen command if we're playing back input, because fullscreen misbehaves during playback sessions.
		// (Video recording breaks, playback machine desktop size might not match recording machine desktop size, etc.)
		if (Controller->InputAutomationComponent && Controller->InputAutomationComponent->IsPlaying())
		{
			return true;
		}

		auto viewport = GetWorld()->GetGameViewport();
		return viewport->HandleToggleFullscreenCommand();
	}
	case EInputCommand::Invert:
	{
		return Controller->HandleInvert();
	}
	case EInputCommand::FlipX:
	case EInputCommand::FlipY:
	case EInputCommand::FlipZ:
	{
		EAxis::Type flipAxis = EAxis::None;

		switch (Command)
		{
		case EInputCommand::FlipX:
			flipAxis = EAxis::X;
			break;
		case EInputCommand::FlipY:
			flipAxis = EAxis::Y;
			break;
		case EInputCommand::FlipZ:
			flipAxis = EAxis::Z;
			break;
		default:
			break;
		}

		bool bHandled = currentTool->HandleFlip(flipAxis);
		if (bHandled)
		{
			static const FString eventName(TEXT("HandleFlip"));
			UModumateAnalyticsStatics::RecordSimpleToolEvent(this, currentTool->GetToolMode(), eventName);
		}

		return bHandled;
	}
	case EInputCommand::OffsetLeft:
	case EInputCommand::OffsetRight:
	case EInputCommand::OffsetUp:
	case EInputCommand::OffsetDown:
	{
		if (!currentTool)
		{
			return false;
		}
		FVector2D viewSpaceDirection(ForceInitToZero);

		switch (Command)
		{
		case EInputCommand::OffsetLeft:
			viewSpaceDirection.Set(-1.0f, 0.0f);
			break;
		case EInputCommand::OffsetRight:
			viewSpaceDirection.Set(1.0f, 0.0f);
			break;
		case EInputCommand::OffsetUp:
			viewSpaceDirection.Set(0.0f, 1.0f);
			break;
		case EInputCommand::OffsetDown:
			viewSpaceDirection.Set(0.0f, -1.0f);
			break;
		}

		bool bHandled = currentTool->HandleOffset(viewSpaceDirection);
		if (bHandled)
		{
			static const FString eventName(TEXT("HandleJustify"));
			UModumateAnalyticsStatics::RecordSimpleToolEvent(this, currentTool->GetToolMode(), eventName);
		}

		return bHandled;
	}


	// Tool activation commands are handled before the switch statement!

	// Tool-related commands

	case EInputCommand::Draw:
	{
		Controller->SetToolCreateObjectMode(EToolCreateObjectMode::Draw);
		return true;
	}

	case EInputCommand::PaintBucket:
	{
		Controller->SetToolCreateObjectMode(EToolCreateObjectMode::Apply);
		return true;
	}

	case EInputCommand::PaintBucketAdd:
	{
		Controller->SetToolCreateObjectMode(EToolCreateObjectMode::Add);
		return true;
	}

	case EInputCommand::OpeningStamp:
	{
		Controller->SetToolCreateObjectMode(EToolCreateObjectMode::Stamp);
		return true;
	}

	case EInputCommand::TerrainPoint:
	{
		Controller->SetToolCreateObjectMode(EToolCreateObjectMode::Draw);
		return true;
	}
	case EInputCommand::SpanEditStart:
	{
		Controller->SetToolCreateObjectMode(EToolCreateObjectMode::SpanEdit);
		return true;
	}
	case EInputCommand::SpanEditConfirm:
	{
		if (Controller->CurrentTool)
		{
			Controller->CurrentTool->CommitSpanEdit();
		}
		return true;
	}
	case EInputCommand::SpanEditCancel:
	{
		if (Controller->CurrentTool)
		{
			Controller->CurrentTool->CancelSpanEdit();
		}
		return true;
	}
	case EInputCommand::SpanJoin:
	{
		UModumateObjectStatics::TryJoinSelectedMetaSpan(GetWorld());
		return true;
	}
	case EInputCommand::SpanSeparate:
	{
		UModumateObjectStatics::SeparateSelectedMetaSpan(GetWorld());
		return true;
	}

	// Non-tool modal commands

	case EInputCommand::NextViewMode:
	{
		return Controller->EMPlayerState->ChangeViewMode(1);
	}
	case EInputCommand::PrevViewMode:
	{
		return Controller->EMPlayerState->ChangeViewMode(-1);
	}
	case EInputCommand::NextDrawingMode:
	{
		return Controller->CurrentTool->CycleMode();
	}
	case EInputCommand::SetViewMode_MetaGraph:
	{
		return Controller->EMPlayerState->SetViewMode(EEditViewModes::MetaGraph);
	}
	case EInputCommand::SetViewMode_Separators:
	{
		return Controller->EMPlayerState->SetViewMode(EEditViewModes::Separators);
	}
	case EInputCommand::SetViewMode_SurfaceGraphs:
	{
		return Controller->EMPlayerState->SetViewMode(EEditViewModes::SurfaceGraphs);
	}
	case EInputCommand::SetViewMode_AllObjects:
	{
		return Controller->EMPlayerState->SetViewMode(EEditViewModes::AllObjects);
	}
	case EInputCommand::SetViewMode_Physical:
	{
		return Controller->EMPlayerState->SetViewMode(EEditViewModes::Physical);
	}
	case EInputCommand::ToggleRoomView:
	{
		Controller->EMPlayerState->ToggleRoomViewMode();
		return true;
	}
	case EInputCommand::ToggleWalkAround:
	{
		Controller->ToggleGravityPawn();
		return true;
	}
	case EInputCommand::SnapNextAxisLeft:
	case EInputCommand::SnapNextAxisRight:
	case EInputCommand::SnapNextAxisUp:
	case EInputCommand::SnapNextAxisDown:
	{
		FVector2D nextAxisDirection(ForceInitToZero);
		bool bUseSelection = (Controller->EMPlayerState->SelectedObjects.Num() > 0);

		switch (Command)
		{
		case EInputCommand::SnapNextAxisLeft:
			nextAxisDirection.X = -1.0f;
			break;
		case EInputCommand::SnapNextAxisRight:
			nextAxisDirection.X = 1.0f;
			break;
		case EInputCommand::SnapNextAxisUp:
			nextAxisDirection.Y = 1.0f;
			break;
		case EInputCommand::SnapNextAxisDown:
			nextAxisDirection.Y = -1.0f;
			break;
		}

		return Controller->CameraController->ZoomToNextAxis(nextAxisDirection, bUseSelection);
	}
	default:
	{
		ensureAlwaysMsgf(false, TEXT("Invalid/unhandled input command: %s"), *GetEnumValueString(Command));
		return false;
	}
	}
}

bool UEditModelInputHandler::TryCommand(EInputCommand Command)
{
	bool bSuccess = TryCommandInternal(Command);
	if (bSuccess)
	{
		OnExecutedCommand.Broadcast(Command);

		FString analyticsEventName = FString::Printf(TEXT("Command_%s"), *GetEnumValueString(Command));
		UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Input, analyticsEventName);
	}

	return bSuccess;
}

void UEditModelInputHandler::DoCommand(EInputCommand Command)
{
	TryCommand(Command);
}

bool UEditModelInputHandler::GetInputCommandData(EInputCommand InputCommand, FInputCommandData &OutData) const
{
	if (AllInputCommandData.Contains(InputCommand))
	{
		OutData = AllInputCommandData[InputCommand];
		return true;
	}

	return false;
}

bool UEditModelInputHandler::GetInputCommandBinding(EInputCommand InputCommand, int32 BindingIndex, TArray<FInputChord> &OutBinding, TArray<FText> &OutKeyTexts) const
{
	const FInputCommandData *CommandData = AllInputCommandData.Find(InputCommand);

	if (CommandData && CommandData->AllBindings.IsValidIndex(BindingIndex))
	{
		OutBinding = CommandData->AllBindings[BindingIndex];

		OutKeyTexts.Reset();
		for (const FInputChord &chord : OutBinding)
		{
			OutKeyTexts.Add(chord.GetInputText());
		}

		return true;
	}

	return false;
}

EToolMode UEditModelInputHandler::ToolModeFromInputCommand(EInputCommand InputCommand)
{
	uint8 commandIdx = (uint8)InputCommand;
	uint8 toolStartIdx = (uint8)EInputCommand::ActivateTool_START;
	uint8 toolEndIdx = (uint8)EInputCommand::ActivateTool_END;
	if ((commandIdx > toolStartIdx) && (commandIdx < toolEndIdx))
	{
		uint8 toolModeIdx = commandIdx - toolStartIdx;
		return (EToolMode)toolModeIdx;
	}

	return EToolMode::VE_NONE;
}

EInputCommand UEditModelInputHandler::InputCommandFromToolMode(EToolMode ToolMode)
{
	if (ToolMode != EToolMode::VE_NONE)
	{
		uint8 toolModeIdx = (uint8)ToolMode;
		uint8 toolStartIdx = (uint8)EInputCommand::ActivateTool_START;
		uint8 toolEndIdx = (uint8)EInputCommand::ActivateTool_END;

		uint8 commandIdx = toolStartIdx + toolModeIdx;
		return (EInputCommand)commandIdx;
	}

	return EInputCommand::None;
}

void UEditModelInputHandler::RequestInputDisabled(const FName& Requester, bool bShouldDisable)
{
	bool bWasInputEnabled = IsInputEnabled();

	if (bShouldDisable)
	{
		InputDisabledRequests.Add(Requester);
	}
	else
	{
		InputDisabledRequests.Remove(Requester);
	}

	bool bShouldInputBeEnabled = IsInputEnabled();

	if (bShouldInputBeEnabled != bWasInputEnabled)
	{
		SetInputEnabled(bShouldInputBeEnabled);
	}
}

bool UEditModelInputHandler::IsInputEnabled() const
{
	return (InputDisabledRequests.Num() == 0);
}

void UEditModelInputHandler::RequestAxisInputPriority(const FName &Requester, bool bShouldPrioritizeAxisInputs)
{
	if (bShouldPrioritizeAxisInputs)
	{
		bool bDuplicateRequest = false;
		AxisPriorityRequests.Add(Requester, &bDuplicateRequest);
		ensureAlwaysMsgf(!bDuplicateRequest, TEXT("%s already requested that axis input be prioritized!"), *Requester.ToString());
	}
	else
	{
		int32 numRemoved = AxisPriorityRequests.Remove(Requester);
		ensureAlwaysMsgf(numRemoved == 1, TEXT("%s revoked an axis prioritization request that was never set!"), *Requester.ToString());
	}
}

bool UEditModelInputHandler::IsAxisInputPrioritized() const
{
	return (AxisPriorityRequests.Num() > 0);
}

void UEditModelInputHandler::HandleBoundChord(FInputChord Chord)
{
	if (!IsInputEnabled())
	{
		return;
	}

	// When handling any chord, set a timer to reset the current command sequence, so that we don't get stuck.
	GetWorld()->GetTimerManager().SetTimer(SequenceResetTimer, this, &UEditModelInputHandler::OnCommandReset, SequenceResetTime);

	auto curCommandNode = CurCommandNode.Pin();
	if (!curCommandNode.IsValid())
	{
		return;
	}

	// If the current chord isn't in the sequence, then reset the current sequence
	if (!curCommandNode->Children.Contains(Chord))
	{
		CurCommandNode = RootCommandTrie;
		curCommandNode = CurCommandNode.Pin();

		// Allow starting a new sequence from the beginning, with the new chord
		if (ensure(curCommandNode.IsValid()) && curCommandNode->Children.Contains(Chord))
		{
			curCommandNode = CurCommandNode.Pin();
		}
		// But if it's not the start of any chord, then exit with the reset sequence.
		else
		{
			return;
		}
	}

	// If this is a root command that overlaps with a bound axis input, and axis input is currently prioritized,
	// then don't execute the command or traverse deeper in the input trie.
	if ((curCommandNode == RootCommandTrie) && IsAxisInputPrioritized() && BoundAxisKeys.Contains(Chord.Key))
	{
		return;
	}

	CurCommandNode = curCommandNode->Children[Chord];
	curCommandNode = CurCommandNode.Pin();

	// Command nodes shouldn't have any invalid children, they somehow got deleted unexpectedly
	if (!ensure(curCommandNode.IsValid()))
	{
		return;
	}

	// If the current node has a command associated with it, then execute it now
	bool bCommandSuccess = false;
	if (curCommandNode->InputCommand != EInputCommand::None)
	{
		bCommandSuccess = TryCommand(curCommandNode->InputCommand);
	}

	// If the current node has no children, or if it successfully executed and captures input, then we want to start back at the root immediately
	if (bCommandSuccess || (curCommandNode->Children.Num() == 0))
	{
		CurCommandNode = RootCommandTrie;
	}
}

void UEditModelInputHandler::HandleDigitKey(int32 DigitNumber)
{
	if (IsInputEnabled())
	{
		Controller->HandleDigitKey(DigitNumber);
	}
}

void UEditModelInputHandler::OnCommandReset()
{
	CurCommandNode = RootCommandTrie;
}

void UEditModelInputHandler::SetInputEnabled(bool bNewEnabled)
{
	if (!bNewEnabled)
	{
		OnCommandReset();
	}
}

void UEditModelInputHandler::send_command(const FString& CommandString) {
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::send_command"));

	EInputCommand command = GetEnumValueByString<EInputCommand>(CommandString);
	TryCommandInternal(command);
}
