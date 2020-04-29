// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectMacros.h"
#include "Graph/Graph3D.h"

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
#define COLLISION_DECORATOR_MOI		ECC_GameTraceChannel4 


UENUM()
enum class EBIMValueType : uint8
{
	None = 0,
	UserString,
	FixedText,
	Number,
	Integer,
	Bool,
	Color,
	Dimension,
	Material,
	Formula,
	Subcategory,
	Select,
	TableSelect,
	DynamicList,
	Form,
	Error=255
};

UENUM(BlueprintType)
enum class EBIMValueScope : uint8
{
	None = 0,
	Assembly,
	Layer,
	Pattern,
	Module,
	Gap,
	ToeKick,
	Node,
	Mesh,
	Portal,
	MaterialColor,
	Form,
	Preset,
	Room,
	Drawing,
	//NOTE: finish bindings to be refactored, supporting old version as scopes for now
	//Underscores appear in metadata table so maintaining here
	Interior_Finish,
	Exterior_Finish,
	Glass_Finish,
	Frame_Finish,
	Hardware_Finish,
	Cabinet_Interior_Finish,
	Cabinet_Exterior_Finish,
	Cabinet_Glass_Finish,
	Cabinet_Hardware_Finish,
	Error=255
};

UENUM(BlueprintType)
enum class ECraftingNodePresetStatus : uint8
{
	None = 0,
	UpToDate,
	Dirty,
	Pending,
	ReadOnly
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
	VE_SPLIT,
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
	VE_ROOF,
	VE_METAPLANE,
	VE_CUTPLANE,
	VE_SCOPEBOX,
	VE_JOIN,
	VE_CREATESIMILAR,
	VE_STRUCTURELINE,
	VE_DRAWING,
	VE_GRAPH2D
};

UENUM(BlueprintType)
enum class EObjectType : uint8
{
	OTNone = 0,
	OTWallSegment,
	OTRailSegment,
	OTFloorSegment,
	OTRoof,
	OTDoor,
	OTWindow,
	OTFurniture,
	OTCabinet,
	OTStaircase,
	OTFinish,
	OTLineSegment,
	OTGroup,
	OTRoom,
	OTCountertop,
	OTTrim,
	OTGraphVertex,	// DEPRECATED, once meta planes replace the 2D room graph
	OTGraphEdge,	// DEPRECATED, once meta planes replace the 2D room graph
	OTMetaVertex,
	OTMetaEdge,
	OTMetaPlane,
	OTCutPlane,
	OTScopeBox,
	OTStructureLine,
	OTDrawing,
	OTUnknown
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
enum class EPortalFunction : uint8
{
	None,
	CasedOpening,
	Swing,
	Barn,
	Pocket,
	Sliding,
	Folding,
	Fixed,
	Casement,
	Awning,
	Hopper,
	Hung,
	Jalousie,
	Cabinet
};

UENUM(BlueprintType)
enum class EPortalSlotType : uint8
{
	None,
	Frame,
	Panel,
	Hinge,
	Handle,
	Lock,
	Closer,
	Knocker,
	Peephole,
	Protection,
	Silencer,
	Stop,
	DogDoor,
	MailSlot,
	Astragal,
	Sidelite,
	Transom,
	Screen,
	Hole,
	Mullion,
	Track,
	Caster
};

UENUM(BlueprintType)
enum class EDecisionType : uint8
{
	None = 0,
	Form,
	Select,
	YesNo,
	String,
	Dimension,
	TableSelect,
	Derived,
	Private,
	Terminal,
	DynamicList,
	Preset
};

UENUM(BlueprintType)
enum class EEditObjectStatus : uint8
{
	None = 0,
	CanSave,
	CannotSave
};

UENUM(BlueprintType)
enum class ECraftingPresetType : uint8
{
	None = 0,
	User,
	BuiltIn
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
	ConstructionTimeline
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

UENUM(BlueprintType)
enum class EHandleType : uint8
{
	Default,
	Justification
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EObjectDirtyFlags : uint8
{
	None = 0x00,
	Structure = 0x01,
	Mitering = 0x02,
	Visuals = 0x04
};
ENUM_CLASS_FLAGS(EObjectDirtyFlags);

UENUM(BlueprintType)
enum class ECraftingResult : uint8
{
	None = 0,
	Success,
	Error
};

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
enum class EAreaType : uint8
{
	None = 0,
	Net,
	Gross
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

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetTextForObjectType(EObjectType ObjectType, bool bPlural = false);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static ECollisionChannel CollisionTypeFromObjectType(EObjectType ot);

	UFUNCTION(BlueprintPure, Category = "Modumate Types")
	static FText GetAreaTypeText(EAreaType AreaType);

	static const EObjectDirtyFlags OrderedDirtyFlags[3];
};
