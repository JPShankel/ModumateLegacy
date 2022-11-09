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

UENUM()
enum class EPresetPropertyMatrixNames : uint8
{
	None = 0,
	IESLight,
	ConstructionCost,
	MiterPriority,
	PatternRef,
	ProfileRef,
	MeshRef,
	Material,
	Dimensions,
	Slots,
	InputPins,
	Dimension,
	RawMaterial,
	Profile,
	Mesh,
	Slot,
	IESProfile,
	Pattern,
	SlotConfig,
	Part,
	Preset,
	Symbol,
	Error = 255
};

using FBIMNameType = FName;

