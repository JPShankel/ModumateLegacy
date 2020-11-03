// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "BIMKernel/BIMEnums.h"
#include "Database/ModumateObjectEnums.h"

struct  FBIMPropertySheetRecord;
typedef FName FBIMNameType;

FBIMNameType BIMNameFromValueType(EBIMValueType vt);
EBIMValueType  BIMValueTypeFromName(const FBIMNameType &name);

FBIMNameType BIMNameFromValueScope(EBIMValueScope s);
EBIMValueScope BIMValueScopeFromName(const FBIMNameType &name);

struct MODUMATE_API FBIMPropertyValue
{
	typedef Modumate::FModumateCommandParameter FValue;

	FBIMPropertyValue() : Scope(EBIMValueScope::None), Type(EBIMValueType::None), Value(0) {}

	FBIMPropertyValue(EBIMValueScope InScope, EBIMValueType InType, const FBIMNameType &InName);
	FBIMPropertyValue(EBIMValueScope InScope, EBIMValueType InType, const FBIMNameType &InName, const FValue &InValue);

	FBIMPropertyValue(EBIMValueType InType, const FBIMNameType &InQN);
	FBIMPropertyValue(EBIMValueType InType, const FBIMNameType &InQN, const FValue &InValue);

	FBIMPropertyValue(const FBIMNameType &InQN);
	FBIMPropertyValue(const FBIMNameType &InQN, const FValue &InValue);

	FBIMNameType QN() const;

	EBIMValueScope Scope;
	EBIMValueType Type;
	FBIMNameType Name;
	FValue Value;
};

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
	extern const FBIMNameType Configuration;
	extern const FBIMNameType Depth;
	extern const FBIMNameType Diameter;
	extern const FBIMNameType DimensionSetComments;
	extern const FBIMNameType DimensionSetName;
	extern const FBIMNameType Dimensions;
	extern const FBIMNameType EngineMaterial;
	extern const FBIMNameType Extents;
	extern const FBIMNameType Form;
	extern const FBIMNameType Function;
	extern const FBIMNameType HasFace;
	extern const FBIMNameType Height;
	extern const FBIMNameType HexValue;
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
	extern const FBIMNameType Scale;
	extern const FBIMNameType Slope;
	extern const FBIMNameType StyleComments;
	extern const FBIMNameType StyleName;
	extern const FBIMNameType Subcategory;
	extern const FBIMNameType Tag;
	extern const FBIMNameType Tangent;
	extern const FBIMNameType Thickness;
	extern const FBIMNameType Title;
	extern const FBIMNameType ToeKickDepth;
	extern const FBIMNameType ToeKickHeight;
	extern const FBIMNameType TrimProfile;
	extern const FBIMNameType UseGroupType;
	extern const FBIMNameType Width;
}

class MODUMATE_API FBIMPropertySheet : public Modumate::FModumateFunctionParameterSet
{
private:
	TMap<FBIMNameType, TArray<FBIMNameType>> PropertyBindings;
public:
	// Scope-aware 'Property' set/get helper functions
	// Use get/set value in parent for qualified variable strings (ie 'Module.Count')
	FBIMPropertyValue::FValue GetProperty(EBIMValueScope Scope, const FBIMNameType& Name) const;
	void SetProperty(EBIMValueScope Scope, const FBIMNameType& Name, const FBIMPropertyValue::FValue& Param);
	bool HasProperty(EBIMValueScope Scope, const FBIMNameType& Name) const;

	void RemoveProperty(EBIMValueScope Scope, const FBIMNameType& Name);

	// When a target property is bound to a source, it is updated with the value of the source
	bool BindProperty(EBIMValueScope SourceScope, const FBIMNameType& SourceName, EBIMValueScope TargetScope, const FBIMNameType& TargetName);

	template<class T>
	bool TryGetProperty(EBIMValueScope Scope, const FBIMNameType& Name, T& OutT) const
	{
		if (HasProperty(Scope, Name))
		{
			OutT = GetProperty(Scope, Name);
			return true;
		}
		return false;
	}

	bool FromDataRecord(const FBIMPropertySheetRecord& InRecord);
	bool ToDataRecord(FBIMPropertySheetRecord& OutRecord) const;
};