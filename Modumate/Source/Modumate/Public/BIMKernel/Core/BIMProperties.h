// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "ModumateCore/ModumateUnits.h"
#include "Objects/ModumateObjectEnums.h"
#include "BIMProperties.generated.h"

struct  FBIMPropertySheetRecord;
typedef FName FBIMNameType;

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

FBIMNameType BIMNameFromValueType(EBIMValueType vt);
EBIMValueType  BIMValueTypeFromName(const FBIMNameType &name);

FBIMNameType BIMNameFromValueScope(EBIMValueScope s);
EBIMValueScope BIMValueScopeFromName(const FBIMNameType &name);

// TODO: Re-evaulate as an enum if/when variable list stabilizes and no custom variables are required
namespace BIMPropertyNames
{
	extern const FBIMNameType Angle;
	extern const FBIMNameType Area;
	extern const FBIMNameType AreaType;
	extern const FBIMNameType AssetID;
	extern const FBIMNameType AssetPath;
	extern const FBIMNameType BevelWidth;
	extern const FBIMNameType Category;
	extern const FBIMNameType Code;
	extern const FBIMNameType Color;
	extern const FBIMNameType Comments;
	extern const FBIMNameType ConceptualSizeY;
	extern const FBIMNameType Configuration;
	extern const FBIMNameType CraftingIconAssetFilePath;
	extern const FBIMNameType DefaultValue;
	extern const FBIMNameType Depth;
	extern const FBIMNameType Description;
	extern const FBIMNameType Diameter;
	extern const FBIMNameType DimensionSetComments;
	extern const FBIMNameType DimensionSetName;
	extern const FBIMNameType Dimensions;
	extern const FBIMNameType DisplayName;
	extern const FBIMNameType EngineMaterial;
	extern const FBIMNameType Extents;
	extern const FBIMNameType Form;
	extern const FBIMNameType Function;
	extern const FBIMNameType HasFace;
	extern const FBIMNameType Height;
	extern const FBIMNameType HexValue;
	extern const FBIMNameType ID;
	extern const FBIMNameType Length;
	extern const FBIMNameType LoadFactorSpecialCalc;
	extern const FBIMNameType MaterialKey;
	extern const FBIMNameType MaterialColor;
	extern const FBIMNameType Mesh;
	extern const FBIMNameType ModuleCount;
	extern const FBIMNameType Name;
	extern const FBIMNameType NamedDimensions;
	extern const FBIMNameType Normal;
	extern const FBIMNameType Number;
	extern const FBIMNameType OccupantLoadFactor;
	extern const FBIMNameType OccupantsNumber;
	extern const FBIMNameType Overhang;
	extern const FBIMNameType Override;
	extern const FBIMNameType PartSet;
	extern const FBIMNameType Pattern;
	extern const FBIMNameType Preset;
	extern const FBIMNameType ProfileKey;
	extern const FBIMNameType Recess;
	extern const FBIMNameType Scale;
	extern const FBIMNameType Slope;
	extern const FBIMNameType StyleComments;
	extern const FBIMNameType StyleName;
	extern const FBIMNameType Subcategory;
	extern const FBIMNameType SupportedNCPs;
	extern const FBIMNameType Tag;
	extern const FBIMNameType Tangent;
	extern const FBIMNameType Thickness;
	extern const FBIMNameType Title;
	extern const FBIMNameType ToeKickDepth;
	extern const FBIMNameType ToeKickHeight;
	extern const FBIMNameType TreadDepthIdeal;
	extern const FBIMNameType TrimProfile;
	extern const FBIMNameType UIGroup;
	extern const FBIMNameType UseGroupType;
	extern const FBIMNameType Width;
	extern const FBIMNameType Zalign;
}

struct MODUMATE_API FBIMPropertyKey
{
	FBIMPropertyKey() {}

	FBIMPropertyKey(EBIMValueScope InScope, const FBIMNameType& InName);
	FBIMPropertyKey(const FBIMNameType& InQN);

	FBIMNameType QN() const;

	EBIMValueScope Scope = EBIMValueScope::None;
	FBIMNameType Name;
};

USTRUCT()
struct MODUMATE_API FBIMPropertySheet
{
	GENERATED_BODY()

private:
	// BIM properties come in many flavors but are all either numbers or strings
	typedef TMap<FName, FString> FStringMap;
	typedef TMap<FName, float> FNumberMap;

	UPROPERTY()
	TMap<FName, FString> StringMap;

	UPROPERTY()
	TMap<FName, float> NumberMap;

	// Typed map accessors used in template functions below
	void GetMap(FStringMap*& OutMapPtr){OutMapPtr = &StringMap;}
	void GetMap(FNumberMap*& OutMapPtr){OutMapPtr = &NumberMap;}

	void GetMap(const FStringMap*& OutMapPtr) const {OutMapPtr = &StringMap;}
	void GetMap(const FNumberMap*& OutMapPtr) const {OutMapPtr = &NumberMap;}

public:

	template<class T>
	bool HasProperty(EBIMValueScope InScope, const FBIMNameType& InName) const
	{
		FBIMNameType qn = FBIMPropertyKey(InScope, InName).QN();
		const TMap<FName, T>* propMap;
		GetMap(propMap);
		return propMap->Contains(qn);
	}

	template<class T>
	void SetProperty(EBIMValueScope InScope, const FBIMNameType& InName, const T& InValue)
	{
		TMap<FName, T>* propMap;
		GetMap(propMap);
		propMap->Add(FBIMPropertyKey(InScope,InName).QN(), InValue);
	}

	template<class T>
	T GetProperty(EBIMValueScope Scope, const FBIMNameType& Name) const
	{
		T ret;
		TryGetProperty<T>(Scope, Name, ret);
		return ret;
	}

	template<class T>
	bool TryGetProperty(EBIMValueScope InScope, const FBIMNameType& InName, T& OutT) const
	{
		const TMap<FName, T>* propMap;
		GetMap(propMap);

		const T* ret = propMap->Find(FBIMPropertyKey(InScope, InName).QN());
		if (ret != nullptr)
		{
			OutT = *ret;
			return true;
		}

		return false;
	}

	void ForEachProperty(const TFunction<void(const FBIMPropertyKey& Key, float Value)>& InFunc) const;
	void ForEachProperty(const TFunction<void(const FBIMPropertyKey& Key, const FString& Value)>& InFunc) const;

	// These overrides provide common conversions between text and numerical types using template func
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType& Name, FName& OutT) const;
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType& Name, FText& OutT) const;
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType& Name, FGuid& OutT) const;
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType& Name, FModumateUnitValue& OutT) const;
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType& Name, int32& OutT) const;
	
	EBIMResult AddProperties(const FBIMPropertySheet& PropSheet);

	bool operator==(const FBIMPropertySheet& RHS) const;
	bool operator!=(const FBIMPropertySheet& RHS) const;
};