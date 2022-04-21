// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ToolsAndAdjustments/Common/ModumateToolEnums.h"

#include "ModumateObjectEnums.generated.h"

// Collision defines, for easier usage in code.
// MUST BE CONSISTENT WITH DEFINITIONS IN DefaultEngine.ini!
#define COLLISION_HANDLE			ECC_GameTraceChannel1
#define COLLISION_DEFAULT_MOI		ECC_GameTraceChannel2 
#define COLLISION_META_MOI			ECC_GameTraceChannel3 
#define COLLISION_SURFACE_MOI		ECC_GameTraceChannel4 
#define COLLISION_DECORATOR_MOI		ECC_GameTraceChannel5 

static constexpr int32 MOD_ID_NONE = 0;

UENUM(BlueprintType)
enum class EObjectType : uint8
{
	OTNone = 0,
	OTWallSegment,
	OTRailSegment,
	OTFloorSegment,
	OTRoofFace,
	OTDoor,
	OTWindow,
	OTFurniture,
	OTCabinet,
	OTStaircase,
	OTFinish,
	OTRoom,
	OTCountertop,
	OTTrim,
	OTMetaVertex,
	OTMetaEdge,
	OTMetaEdgeSpan,
	OTMetaPlane,
	OTMetaPlaneSpan,
	OTSurfaceGraph,
	OTSurfaceVertex,
	OTSurfaceEdge,
	OTSurfacePolygon,
	OTCutPlane,
	OTScopeBox,
	OTStructureLine,
	OTDrawing,
	OTRoofPerimeter,
	OTCeiling,
	OTSystemPanel,
	OTMullion,
	OTPointHosted,
	OTEdgeHosted,
	OTBackgroundImage,
	OTEdgeDetail,
	OTTerrain,
	OTTerrainVertex,
	OTTerrainEdge,
	OTTerrainPolygon,
	OTTerrainMaterial,
	OTCameraView,
	OTMetaGraph,
	OTDesignOption,
	OTFaceHosted,
	OTPattern2D,
	OTUnknown
};

UENUM(BlueprintType)
enum class EEditViewModes : uint8
{
	MetaGraph,
	Separators,
	SurfaceGraphs,
	AllObjects,
	Physical,
};

UENUM(BlueprintType)
enum class ESelectObjectMode : uint8
{
	DefaultObjects,
	Advanced,
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EObjectDirtyFlags : uint8
{
	None		= 0		UMETA(Hidden),
	Structure	= 1 << 0,
	Mitering	= 1 << 1,
	Visuals		= 1 << 2,

	All = Structure | Mitering | Visuals	UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EObjectDirtyFlags);

UENUM(BlueprintType)
enum class EAreaType : uint8
{
	None = 0,
	Net,
	Gross
};


