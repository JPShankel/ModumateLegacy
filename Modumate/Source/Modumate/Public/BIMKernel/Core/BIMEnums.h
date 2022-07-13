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
enum class EPresetMeasurementMethod : uint8
{
	None = 0,
	Part,
	PartBySizeGroup,
	FFE,
	ModuleLinear,
	ModulePlanar,
	ModuleVolumetric,
	GapLinear,
	GapPlanar,
	GapVolumetric,
	Layer,
	LayerMass,
	AssemblyLayered,
	AssemblyLinear,
	AssemblyRigged,
	AssemblyStair,
	AssemblyCabinet,
	Error = 255
};

UENUM()
enum class EBIMAssetType : uint8
{
	None,
	Mesh,
	RawMaterial,
	Profile,
	Pattern,
	Material,
	Color,
	IESProfile,
	Error = 255
};
