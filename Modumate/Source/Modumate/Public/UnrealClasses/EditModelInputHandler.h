// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Framework/Commands/InputChord.h"
#include "Objects/ModumateObjectEnums.h"
#include "UnrealClasses/EditModelPlayerController.h"

#include "EditModelInputHandler.generated.h"


class AEditModelGameState;

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
	MainMenu,
	Settings,
	Exit,
	Undo,
	Redo,
	TakeScreenshot,
	SavePDF,
	EscapeKey,

	// General object commands
	Copy,
	Cut,
	Paste,
	SelectAll,
	SelectInverse,
	Deselect,
	DeleteDefault,
	DeleteOnlySelected,
	HideSelected,
	UnhideAll,
	ToggleUserSnapPoint,
	ZoomExtents,
	ZoomSelected,
	ToggleFullscreen,

	Invert,
	FlipX,
	FlipY,
	FlipZ,
	OffsetLeft,
	OffsetRight,
	OffsetUp,
	OffsetDown,

	// Tool activation commands
	// MUST BE CONSISTENT WITH EToolMode!
	ActivateTool_START,
	ActivateTool_Select,
	ActivateTool_FFE,
	ActivateTool_Move,
	ActivateTool_Rotate,
	ActivateTool_Scale,
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
	ActivateTool_Line,
	ActivateTool_Rectangle,
	ActivateTool_CutPlane,
	ActivateTool_ScopeBox,
	ActivateTool_Join,
	ActivateTool_CreateSimilar,
	ActivateTool_Copy,
	ActivateTool_Paste,
	ActivateTool_StructureLine,
	ActivateTool_Drawing,
	ActivateTool_Graph2D,
	ActivateTool_SurfaceGraph,
	ActivateTool_Ceiling,
	ActivateTool_Panel,
	ActivateTool_Mullion,
	ActivateTool_PointHosted,
	ActivateTool_EdgeHosted,
	ActivateTool_FaceHosted,
	ActivateTool_BackgroundImage,
	ActivateTool_Terrain,
	ActivateTool_Group,
	ActivateTool_Ungroup,
	ActivateTool_Pattern2D,
	ActivateTool_END,

	// Tool-related commands
	Draw,
	PaintBucket,
	PaintBucketAdd,
	OpeningStamp,
	OpeningSystem,
	TerrainPoint,
	SpanEditStart,
	SpanEditConfirm,
	SpanEditCancel,

	// Non-tool modal commands
	NextViewMode,
	PrevViewMode,
	NextDrawingMode,
	SetViewMode_MetaGraph,
	SetViewMode_Separators,
	SetViewMode_SurfaceGraphs,
	SetViewMode_AllObjects,
	SetViewMode_Physical,
	ToggleRoomView,
	ToggleWalkAround,

	// Advanced camera commands
	SnapNextAxisLeft,
	SnapNextAxisRight,
	SnapNextAxisUp,
	SnapNextAxisDown,
};

// The struct for the input command data table; there can be multiple rows for a given command,
// so that both key chords and key sequences can be valid bindings for the same command.
USTRUCT(BlueprintType)
struct MODUMATE_API FInputCommandDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInputCommand Command = EInputCommand::None;

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputCommandDelegate, EInputCommand, ExecutedCommand);
DECLARE_DELEGATE_OneParam(FInputChordDelegate, FInputChord);
DECLARE_DELEGATE_OneParam(FInputDigitDelegate, int32);

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
	UEditModelInputHandler();
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
	class AEditModelPlayerController *Controller;

	UPROPERTY()
	class AEditModelGameState *GameState;

	UPROPERTY(EditAnywhere)
	UDataTable *InputCommandDataTable;

	UPROPERTY(EditAnywhere)
	float SequenceResetTime;

	UPROPERTY()
	TMap<EInputCommand, FInputCommandData> AllInputCommandData;

	UPROPERTY()
	FInputCommandDelegate OnExecutedCommand;

	void RequestInputDisabled(const FName& Requester, bool bShouldDisable);
	bool IsInputEnabled() const;

	void RequestAxisInputPriority(const FName& Requester, bool bShouldPrioritizeAxisInputs);
	bool IsAxisInputPrioritized() const;

	UFUNCTION()
	void send_command(const FString& CommandString);

protected:
	UFUNCTION()
	void HandleBoundChord(FInputChord Chord);

	UFUNCTION()
	void HandleDigitKey(int32 DigitNumber);

	UFUNCTION()
	void OnCommandReset();

	void SetInputEnabled(bool bNewEnabled);
	bool TryCommandInternal(EInputCommand Command);

	UPROPERTY()
	TArray<FInputChord> PendingPressedChords;

	TWeakPtr<FCommandTrieNode> CurCommandNode;
	TSharedPtr<FCommandTrieNode> RootCommandTrie;
	TSet<FInputChord> BoundChords;
	FTimerHandle SequenceResetTimer;
	TSet<FName> InputDisabledRequests;
	TSet<FName> AxisPriorityRequests;
	TSet<FKey> BoundAxisKeys;
};
