// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Graph/Graph2DTypes.h"
#include "Graph/Graph3DTypes.h"
#include "ModumateCore/EnumHelpers.h"
#include "UObject/ObjectMacros.h"

#include "ModumateObjectEnums.generated.h"


// Collision defines, for easier usage in code.
// MUST BE CONSISTENT WITH DEFINITIONS IN DefaultEngine.ini!
#define COLLISION_HANDLE			ECC_GameTraceChannel1
#define COLLISION_DEFAULT_MOI		ECC_GameTraceChannel2 
#define COLLISION_META_MOI			ECC_GameTraceChannel3 
#define COLLISION_SURFACE_MOI		ECC_GameTraceChannel4 
#define COLLISION_DECORATOR_MOI		ECC_GameTraceChannel5 

UENUM()
enum class EMOIDeltaType : uint8
{
	None = 0,
	Mutate,
	Create,
	Destroy
};

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
};

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
	OTGroup,
	OTRoom,
	OTCountertop,
	OTTrim,
	OTMetaVertex,
	OTMetaEdge,
	OTMetaPlane,
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
	OTUnknown
};


UENUM(BlueprintType)
enum class EEditObjectStatus : uint8
{
	None = 0,
	CanSave,
	CannotSave
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
enum class EToolCreateObjectMode : uint8
{
	Draw,
	Apply,
	Stamp,
	Add
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

UCLASS()
class MODUMATE_API UModumateTypeStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EObjectType ObjectTypeFromToolMode(EToolMode tm);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EToolMode ToolModeFromObjectType(EObjectType ot);

	static EGraph3DObjectType Graph3DObjectTypeFromObjectType(EObjectType ot);
	static EObjectType ObjectTypeFromGraph3DType(EGraph3DObjectType GraphType);

	static EGraphObjectType Graph2DObjectTypeFromObjectType(EObjectType ObjectType);
	static EObjectType ObjectTypeFromGraph2DType(EGraphObjectType GraphType, EToolCategories GraphCategory);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetTextForObjectType(EObjectType ObjectType, bool bPlural = false);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static ECollisionChannel CollisionTypeFromObjectType(EObjectType ot);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetAreaTypeText(EAreaType AreaType);

	static const EObjectDirtyFlags OrderedDirtyFlags[3];

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EToolCategories GetToolCategory(EToolMode ToolMode);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static EToolCategories GetObjectCategory(EObjectType ObjectType);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetToolCategoryText(EToolCategories ToolCategory);
};
