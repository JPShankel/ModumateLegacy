// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateToolEnums.generated.h"

UENUM(BlueprintType)
enum class EToolMode : uint8
{
	VE_NONE,
	VE_SELECT,
	VE_PLACEOBJECT,
	VE_MOVEOBJECT,
	VE_ROTATE,
	VE_SCALE,
	VE_WALL,
	VE_FLOOR,
	VE_DOOR,
	VE_WINDOW,
	VE_STAIR,
	VE_RAIL,
	VE_CABINET,
	VE_WAND,
	VE_FINISH,
	VE_COUNTERTOP,
	VE_TRIM,
	VE_ROOF_FACE,
	VE_ROOF_PERIMETER,
	VE_LINE,
	VE_RECTANGLE,
	VE_CUTPLANE,
	VE_SCOPEBOX,
	VE_JOIN,
	VE_CREATESIMILAR,
	VE_COPY,
	VE_PASTE,
	VE_STRUCTURELINE,
	VE_DRAWING,
	VE_GRAPH2D,
	VE_SURFACEGRAPH,
	VE_CEILING,
	VE_PANEL,
	VE_MULLION,
	VE_POINTHOSTED,
	VE_EDGEHOSTED,
	VE_BACKGROUNDIMAGE,
	VE_TERRAIN,
	VE_GROUP,
	VE_UNGROUP,
	VE_PATTERN2D
};

UENUM(BlueprintType)
enum class EToolCreateObjectMode : uint8
{
	Draw,
	Apply,
	Stamp,
	Add
};

UENUM(BlueprintType)
enum class EToolCategories : uint8
{
	Unknown,
	MetaGraph,
	Separators,
	SurfaceGraphs,
	Attachments,
	SiteTools
};