// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelInputHandler.h"

#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerPawn_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "GameFramework/InputSettings.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Database/ModumateDataTables.h"
#include "ModumateCore/ModumateFunctionLibrary.h"


using namespace Modumate;

UEditModelInputHandler::UEditModelInputHandler(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SequenceResetTime(1.0f)
{
}

void UEditModelInputHandler::BeginPlay()
{
	Super::BeginPlay();

	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
}

void UEditModelInputHandler::SetupBindings()
{
	Controller = Cast<AEditModelPlayerController_CPP>(GetOwner());
	UInputSettings *inputSettings = UInputSettings::GetInputSettings();

	if (ensureAlways(Controller && Controller->InputComponent && inputSettings && InputCommandDataTable))
	{
		RootCommandTrie = MakeShareable(new FCommandTrieNode());
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
			data.ActionName = FName(*EnumValueString(EInputCommand, data.Command));
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
						curTrieNode->Children.Add(curChord, MakeShareable(new FCommandTrieNode()));
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

bool UEditModelInputHandler::TryCommand(EInputCommand Command)
{
	// Special case non-switch handling for tools, for convenience
	EToolMode commandToolMode = ToolModeFromInputCommand(Command);
	if (commandToolMode != EToolMode::VE_NONE)
	{
		Controller->SetToolMode(commandToolMode);
		return true;
	}

	switch (Command)
	{
		// General app and I/O commands

	case EInputCommand::New:
	{
		Controller->NewModel();
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
	case EInputCommand::Undo:
	{
		if (Controller->IsCraftingWidgetActive())
		{
			return false;
		}
		else
		{
			Controller->ClearTextInputs();
			Controller->ModumateCommand(FModumateCommand(Commands::kUndo));
			return true;
		}
	}
	case EInputCommand::Redo:
	{
		if (Controller->IsCraftingWidgetActive())
		{
			return false;
		}
		else
		{
			Controller->ClearTextInputs();
			Controller->ModumateCommand(FModumateCommand(Commands::kRedo));
			return true;
		}
	}
	case EInputCommand::TakeScreenshot:
	{
		return Controller->TakeScreenshot();
	}
	case EInputCommand::SavePDF:
	{
		Controller->TrySavePDF();
		return true;
	}

	// General object commands

	case EInputCommand::Copy:
	{
		Controller->ModumateCommand(FModumateCommand(Commands::kCopySelected));
		return true;
	}
	case EInputCommand::Cut:
	{
		Controller->ModumateCommand(FModumateCommand(Commands::kCutSelected));
		return true;
	}
	case EInputCommand::Paste:
	{
		Controller->ModumateCommand(FModumateCommand(Commands::kPaste));
		return true;
	}
	case EInputCommand::SelectAll:
	{
		Controller->ModumateCommand(FModumateCommand(Commands::kSelectAll));
		return true;
	}
	case EInputCommand::SelectInverse:
	{
		Controller->ModumateCommand(FModumateCommand(Commands::kSelectInverse));
		return true;
	}
	case EInputCommand::Deselect:
	{
		Controller->ModumateCommand(FModumateCommand(Commands::kDeselectAll));
		return true;
	}
	case EInputCommand::DeleteDefault:
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kDeleteSelectedObjects)
			.Param(Parameters::kIncludeConnected, true)
		);
		return true;
	}
	case EInputCommand::DeleteOnlySelected:
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kDeleteSelectedObjects)
			.Param(Parameters::kIncludeConnected, false)
		);
		return true;
	}
	case EInputCommand::GroupSelected:
	{
		Controller->GroupSelected(true);
		return true;
	}
	case EInputCommand::UngroupSelected:
	{
		Controller->GroupSelected(false);
		return true;
	}
	case EInputCommand::HideSelected:
	{
		// TODO: move this logic somewhere reasonable
		TArray<AActor*> selectedObjActors;
		Controller->EMPlayerState->GetSelectorModumateObjects(selectedObjActors);
		if (selectedObjActors.Num() > 0)
		{
			UModumateFunctionLibrary::DocAddHideMoiActors(selectedObjActors);
			Controller->DeselectAll();
			return true;
		}
		// TODO: remove this inconsistent behavior of activating a tool if a fire-and-forget action failed
		else
		{
			Controller->SetToolMode(EToolMode::VE_WAND);
			return true;
		}
	}
	case EInputCommand::UnhideAll:
	{
		// TODO: move this logic somewhere reasonable
		Controller->SetToolMode(EToolMode::VE_SELECT);
		UModumateFunctionLibrary::DocUnHideAllMoiActors(GetOwner());
		return true;
	}
	case EInputCommand::ToggleUserSnapPoint:
	{
		Controller->ToggleUserSnapPoint();
		return true;
	}
	case EInputCommand::ZoomExtents:
	{
		return Controller->CameraController->ZoomToProjectExtents();
	}
	case EInputCommand::ZoomSelected:
	{
		return Controller->CameraController->ZoomToSelection();
	}
	case EInputCommand::Invert:
	{
		return Controller->HandleInvert();
	}

	// Tool activation commands are handled before the switch statement!

	// Tool-related commands

	case EInputCommand::MetaPlaneLine:
	{
		Controller->SetToolMode(EToolMode::VE_METAPLANE);
		Controller->SetToolAxisConstraint(EAxisConstraint::None);
		return true;
	}

	case EInputCommand::MetaPlaneHorizontal:
	{
		Controller->SetToolMode(EToolMode::VE_METAPLANE);
		Controller->SetToolAxisConstraint(EAxisConstraint::AxesXY);
		return true;
	}

	case EInputCommand::MetaPlaneVertical:
	{
		Controller->SetToolMode(EToolMode::VE_METAPLANE);
		Controller->SetToolAxisConstraint(EAxisConstraint::AxisZ);
		return true;
	}

	case EInputCommand::ConstraintAxesNone:
	{
		Controller->SetToolAxisConstraint(EAxisConstraint::None);
		return true;
	}

	case EInputCommand::ConstraintAxesXY:
	{
		Controller->SetToolAxisConstraint(EAxisConstraint::AxesXY);
		return true;
	}

	case EInputCommand::ConstraintAxesZ:
	{
		Controller->SetToolAxisConstraint(EAxisConstraint::AxisZ);
		return true;
	}

	// Non-tool modal commands

	case EInputCommand::CycleEditModes:
	{
		switch (Controller->EMPlayerState->GetSelectedViewMode())
		{
		case EEditViewModes::ObjectEditing:
			Controller->EMPlayerState->SetEditViewModeDirect(EEditViewModes::MetaPlanes);
			return true;
		case EEditViewModes::MetaPlanes:
			Controller->EMPlayerState->SetEditViewModeDirect(EEditViewModes::ObjectEditing);
			return true;
		default:
			return false;
		}
	}
	case EInputCommand::SetEditMode_ObjectEditing:
	{
		Controller->EMPlayerState->SetEditViewModeDirect(EEditViewModes::ObjectEditing);
		return true;
	}
	case EInputCommand::SetEditMode_MetaPlanes:
	{
		Controller->EMPlayerState->SetEditViewModeDirect(EEditViewModes::MetaPlanes);
		return true;
	}
	case EInputCommand::SetEditMode_Rooms:
	{
		Controller->EMPlayerState->SetEditViewModeDirect(EEditViewModes::Rooms);
		return true;
	}
	case EInputCommand::ToggleRoomView:
	{
		Controller->EMPlayerState->ToggleRoomViewMode();
		return true;
	}
	case EInputCommand::ToggleWalkAround:
	{
		Controller->EMPlayerPawn->ToggleWalkAround();
		return true;
	}
	default:
	{
		ensureAlwaysMsgf(false, TEXT("Invalid/unhandled input command: %s"), *EnumValueString(EInputCommand, Command));
		return false;
	}
	}
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

void UEditModelInputHandler::RequestInputDisabled(const FName &Requester, bool bShouldDisable)
{
	bool bWasInputEnabled = IsInputEnabled();

	if (bShouldDisable)
	{
		bool bDuplicateRequest = false;
		InputDisabledRequests.Add(Requester, &bDuplicateRequest);
		ensureAlwaysMsgf(!bDuplicateRequest, TEXT("%s already requested that input be disabled!"), *Requester.ToString());
	}
	else
	{
		int32 numRemoved = InputDisabledRequests.Remove(Requester);
		ensureAlwaysMsgf(numRemoved == 1, TEXT("%s revoked an input disabled request that was never set!"), *Requester.ToString());
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

	if (!curCommandNode->Children.Contains(Chord))
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
	if (curCommandNode->InputCommand != EInputCommand::None)
	{
		TryCommand(curCommandNode->InputCommand);
	}

	// If the current node has no children, then we want to start back at the root immediately
	if (curCommandNode->Children.Num() == 0)
	{
		CurCommandNode = RootCommandTrie;
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
