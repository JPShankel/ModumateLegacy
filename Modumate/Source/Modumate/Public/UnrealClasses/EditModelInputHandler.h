// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Framework/Commands/InputChord.h"
#include "Database/ModumateObjectEnums.h"

#include "EditModelInputHandler.generated.h"


class AEditModelGameState_CPP;
class AEditModelPlayerController_CPP;

// The list of all explicit input commands that can be mapped to a key chord and/or key sequence.
// This list is a Blueprintable UENUM so that they can be activated and referenced directly by UI buttons.
// That way, only EditModelInputHandler needs to understand how to map action intention to other systems.
UENUM(BlueprintType)
enum class EInputCommand : uint8
{
	None,

	// General app and I/O commands
	New,
	Open,
	Save,
	SaveAs,
	Undo,
	Redo,
	TakeScreenshot,
	SavePDF,

	// General object commands
	Copy,
	Cut,
	Paste,
	SelectAll,
	SelectInverse,
	Deselect,
	DeleteDefault,
	DeleteOnlySelected,
	GroupSelected,
	UngroupSelected,
	HideSelected,
	UnhideAll,
	ToggleUserSnapPoint,
	ZoomExtents,
	ZoomSelected,
	Invert,

	// Tool activation commands
	// MUST BE CONSISTENT WITH EToolMode!
	ActivateTool_START,
	ActivateTool_Select,
	ActivateTool_FFE,
	ActivateTool_Move,
	ActivateTool_Rotate,
	ActivateTool_Scale,
	ActivateTool_Split,
	ActivateTool_Wall,
	ActivateTool_Floor,
	ActivateTool_Door,
	ActivateTool_Window,
	ActivateTool_Stair,
	ActivateTool_Rail,
	ActivateTool_Cabinet,
	ActivateTool_Wand,
	ActivateTool_Finish,
	ActivateTool_Countertop,
	ActivateTool_Trim,
	ActivateTool_RoofFace,
	ActivateTool_RoofPerimeter,
	ActivateTool_MetaPlane,
	ActivateTool_CutPlane,
	ActivateTool_ScopeBox,
	ActivateTool_Join,
	ActivateTool_CreateSimilar,
	ActivateTool_StructureLine,
	ActivateTool_Drawing,
	ActivateTool_Graph2D,
	ActivateTool_SurfaceGraph,
	ActivateTool_Ceiling,
	ActivateTool_END,

	// Tool-related commands
	MetaPlaneLine,
	MetaPlaneHorizontal,
	MetaPlaneVertical,
	ConstraintAxesNone,
	ConstraintAxesXY,
	ConstraintAxesZ,

	// Non-tool modal commands
	CycleEditModes,
	SetEditMode_ObjectEditing,
	SetEditMode_MetaPlanes,
	SetEditMode_SurfaceGraphs,
	SetEditMode_Rooms,
	ToggleRoomView,
	ToggleWalkAround,
};

// The struct for the input command data table; there can be multiple rows for a given command,
// so that both key chords and key sequences can be valid bindings for the same command.
USTRUCT(BlueprintType)
struct MODUMATE_API FInputCommandDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInputCommand Command;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText EnabledDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisabledDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FInputChord> Binding;
};

// The struct for storing combined data for an input command, with all of its bindings.
USTRUCT(BlueprintType)
struct MODUMATE_API FInputCommandData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EInputCommand Command = EInputCommand::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName ActionName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText EnabledDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FText DisabledDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 NumBindings = 0;

	TArray<TArray<FInputChord>> AllBindings;
};

// The recursive trie node struct, for determining which command to execute for a given key sequence.
struct MODUMATE_API FCommandTrieNode : public TSharedFromThis<FCommandTrieNode>
{
	FCommandTrieNode()
		: InputCommand(EInputCommand::None)
	{ }

	// The command to execute when we reach this node of the trie
	EInputCommand InputCommand;

	// The mapping of each key chord to the next node(s) that could be executed
	TMap<FInputChord, TSharedPtr<FCommandTrieNode>> Children;
};

// Delegate definitions purely for passing stored arguments via UPlayerInput action bindings.
DECLARE_DELEGATE_OneParam(FInputCommandDelegate, EInputCommand);
DECLARE_DELEGATE_OneParam(FInputChordDelegate, FInputChord);


// The central input handler, whose purpose is to:
// - handle mappable input commands via key chords and/or key sequences
// - execute input commands that are requested from the UI, like button presses
// - TODO: handle raw inputs (such as mouse buttons) that get used by tools and other local controls
// - TODO: handle text entry input, based on focus
UCLASS()
class MODUMATE_API UEditModelInputHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UEditModelInputHandler(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;

	void SetupBindings();

	// General dispatcher for all input commands, with a return value to indicate successful handling.
	UFUNCTION(BlueprintCallable)
	bool TryCommand(EInputCommand Command);

	// Wrapper for TryCommand, for function binding purposes
	UFUNCTION()
	void DoCommand(EInputCommand Command);

	// Try to get the Blueprint-readable data about an input command, if it's in the table
	UFUNCTION(BlueprintPure)
	bool GetInputCommandData(EInputCommand InputCommand, FInputCommandData &OutData) const;

	// Try to get a single binding (potentially a key sequence) for a bound input command
	UFUNCTION(BlueprintPure)
	bool GetInputCommandBinding(EInputCommand InputCommand, int32 BindingIndex, TArray<FInputChord> &OutBinding, TArray<FText> &OutKeyTexts) const;

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EToolMode ToolModeFromInputCommand(EInputCommand InputCommand);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EInputCommand InputCommandFromToolMode(EToolMode ToolMode);

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

	UPROPERTY(EditAnywhere)
	UDataTable *InputCommandDataTable;

	UPROPERTY(EditAnywhere)
	float SequenceResetTime;

	UPROPERTY()
	TMap<EInputCommand, FInputCommandData> AllInputCommandData;

	void RequestInputDisabled(const FName &Requester, bool bShouldDisable);
	bool IsInputEnabled() const;

protected:
	UFUNCTION()
	void HandleBoundChord(FInputChord Chord);

	UFUNCTION()
	void OnCommandReset();

	void SetInputEnabled(bool bNewEnabled);

	UPROPERTY()
	TArray<FInputChord> PendingPressedChords;

	TWeakPtr<FCommandTrieNode> CurCommandNode;
	TSharedPtr<FCommandTrieNode> RootCommandTrie;
	TSet<FInputChord> BoundChords;
	FTimerHandle SequenceResetTimer;
	TSet<FName> InputDisabledRequests;
};
