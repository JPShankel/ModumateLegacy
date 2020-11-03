// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMEnums.generated.h"

UENUM(BlueprintType)
enum class EConfiguratorNodeIconType : uint8
{
	None = 0,
	LayeredAssembly,
	Stair,
	OpeningSystem,
	FFEConfig,
	ExtrudedProfile,
	LayerVertical,
	LayerHorizontal,
	Layer,
	Module,
	Gap2D,
	StaticMesh,
	Profile,
	SawtoothStringer,
	StairNosing,
	GraphConfig2D,
	Material,
	SrfTreatment,
	Color,
	Dimension,
	Pattern,
};

UENUM(BlueprintType)
enum class EConfiguratorNodeIconOrientation : uint8
{
	None = 0,
	Horizontal,
	Vertical,
	Inherited
};

UENUM(BlueprintType)
enum class EBIMResult : uint8
{
	None = 0,
	Success,
	Error
};

UENUM(BlueprintType)
enum class ELayerFunction : uint8
{
	None,
	Void,
	Insulation,
	Structure,
	Substrate,
	Membrane,
	// Items below are for finishes, for now
	Adhesive,
	Underlayment,
	Finish,
	Abstract
};

UENUM(BlueprintType)
enum class ELayerFormat : uint8
{
	None,
	Block,
	Board,
	Brick,
	Channel,
	Deck,
	Joist,
	Masonry,
	Mass,
	Panel,
	Plank,
	Roll,
	Spread,
	Stud,
	Sheet,
	Shingle,
	Tile
};

UENUM()
enum class EBIMValueType : uint8
{
	None = 0,
	AssetPath,
	Boolean,
	CategoryPath,
	Dimension,
	DimensionSet,
	DisplayText,
	Formula,
	HexValue,
	Number,
	PresetID,
	String,
	Vector,
	Error = 255
};

UENUM()
enum class EBIMValueScope : uint8
{
	// These scopes will be used to define how variables attached to a preset node will be bound
	None = 0,
	Assembly,
	Layer,
	Part,
	Pattern,
	Profile,
	Module,
	Mesh,
	Gap,
	RawMaterial,
	Material,
	Color,
	Dimension,
	Parent,
	Preset,
	SlotConfig,
	Slot,
	SurfaceTreatment,

	// These scopes are to be deprecated as we replace the BIM property sheets
	Node,
	Room,
	Roof,
	Error = 255
};

UENUM()
enum class EBIMPresetEditorNodeStatus : uint8
{
	None = 0,
	UpToDate,
	Dirty,
	Pending
};
