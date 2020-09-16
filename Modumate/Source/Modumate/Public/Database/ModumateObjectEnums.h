// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/ObjectMacros.h"
#include "Graph/Graph2DTypes.h"
#include "Graph/Graph3DTypes.h"

#include "ModumateObjectEnums.generated.h"


template<typename TEnum>
static FORCEINLINE FName FindEnumValueFullName(const TCHAR* TypeName, TEnum EnumValue)
{
	static const UEnum* enumPtr = FindObject<UEnum>(ANY_PACKAGE, TypeName, true);
	return enumPtr ? enumPtr->GetNameByValue((int64)EnumValue) : NAME_None;
}

template<typename TEnum>
static FORCEINLINE FString FindEnumValueString(const TCHAR* TypeName, TEnum EnumValue)
{
	static const UEnum* enumPtr = FindObject<UEnum>(ANY_PACKAGE, TypeName, true);
	return enumPtr ? enumPtr->GetNameStringByValue((int64)EnumValue) : FString();
}

template<typename TEnum>
static FORCEINLINE TEnum FindEnumValueByName(const TCHAR* TypeName, FName NameValue)
{
	static const UEnum* enumPtr = FindObject<UEnum>(ANY_PACKAGE, TypeName, true);
	return static_cast<TEnum>(enumPtr ? enumPtr->GetValueByName(NameValue) : 0);
}

template<typename TEnum>
static FORCEINLINE bool TryFindEnumValueByName(const TCHAR* TypeName, FName NameValue, TEnum &OutValue)
{
	static const UEnum* enumPtr = FindObject<UEnum>(ANY_PACKAGE, TypeName, true);
	int64 enumIntValue = enumPtr ? enumPtr->GetValueByName(NameValue) : INDEX_NONE;
	if (enumIntValue != INDEX_NONE)
	{
		OutValue = static_cast<TEnum>(enumIntValue);
		return true;
	}
	return false;
}

#define EnumValueFullName(EnumType, EnumValue) FindEnumValueFullName<EnumType>(TEXT(#EnumType), EnumValue)
#define EnumValueString(EnumType, EnumValue) FindEnumValueString<EnumType>(TEXT(#EnumType), EnumValue)
#define EnumValueByString(EnumType, StringValue) FindEnumValueByName<EnumType>(TEXT(#EnumType), FName(*StringValue))
#define TryEnumValueByString(EnumType, StringValue, OutValue) TryFindEnumValueByName<EnumType>(TEXT(#EnumType), FName(*StringValue), OutValue)
#define TryEnumValueByName(EnumType, NameValue, OutValue) TryFindEnumValueByName<EnumType>(TEXT(#EnumType), NameValue, OutValue)


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
	VE_METAPLANE,
	VE_CUTPLANE,
	VE_SCOPEBOX,
	VE_JOIN,
	VE_CREATESIMILAR,
	VE_STRUCTURELINE,
	VE_DRAWING,
	VE_GRAPH2D,
	VE_SURFACEGRAPH,
	VE_CEILING,
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
enum class EAxisConstraint : uint8
{
	None,
	AxisZ,
	AxesXY,
};

UENUM(BlueprintType)
enum class EEditViewModes : uint8
{
	ObjectEditing,
	MetaPlanes,
	Rooms,
	Volumes,
	ConstructionTimeline,
	SurfaceGraphs
};

UENUM(BlueprintType)
enum class EToolCreateObjectMode : uint8
{
	Draw,
	Apply
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
	Attachments
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

	static Modumate::EGraph3DObjectType Graph3DObjectTypeFromObjectType(EObjectType ot);

	static Modumate::EGraphObjectType Graph2DObjectTypeFromObjectType(EObjectType ObjectType);

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
	static FText GetToolCategoryText(EToolCategories ToolCategory);
};
