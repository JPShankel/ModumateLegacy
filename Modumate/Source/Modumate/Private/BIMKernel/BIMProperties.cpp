// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMProperties.h"
#include "DocumentManagement/ModumateSerialization.h"

struct MODUMATE_API FBIMPropertySheetRecord;


FBIMPropertyValue::FBIMPropertyValue(EBIMValueScope InScope, EBIMValueType InType, const FBIMNameType &InName) :
	Scope(InScope),
	Type(InType),
	Name(InName),
	Value(0)
{}

FBIMPropertyValue::FBIMPropertyValue(EBIMValueScope InScope, EBIMValueType InType, const FBIMNameType &InName, const FValue &InValue) :
	Scope(InScope),
	Type(InType),
	Name(InName),
	Value(InValue)
{}

FBIMPropertyValue::FBIMPropertyValue(EBIMValueType InType, const FBIMNameType &InQN) : Type(InType), Value(0)
{
	if (InQN.IsNone())
	{
		Scope = EBIMValueScope::None;
		Name = NAME_None;
	}
	else
	{
		TArray<FString> split;
		InQN.ToString().ParseIntoArray(split, TEXT("."));
		if (ensureAlways(split.Num() == 2))
		{
			Scope = BIMValueScopeFromName(*split[0]);
			Name = *split[1];
		}
	}
}

FBIMPropertyValue::FBIMPropertyValue(EBIMValueType InType, const FBIMNameType &InQN, const FValue &InValue) : FBIMPropertyValue(InType,InQN)
{
	Value = InValue;
}

FBIMPropertyValue::FBIMPropertyValue(const FBIMNameType &InQN) : FBIMPropertyValue(EBIMValueType::None, InQN)
{}

FBIMPropertyValue::FBIMPropertyValue(const FBIMNameType &InQN, const FValue &InValue) : FBIMPropertyValue(EBIMValueType::None, InQN, InValue)
{}

FBIMNameType BIMNameFromValueType(EBIMValueType InValueType)
{
	return *EnumValueString(EBIMValueType, InValueType);
}

EBIMValueType BIMValueTypeFromName(const FBIMNameType &InName)
{
	return EnumValueByString(EBIMValueType, InName.ToString());
}

FBIMNameType BIMNameFromValueScope(EBIMValueScope InScope)
{
	return *EnumValueString(EBIMValueScope, InScope);
}

EBIMValueScope BIMValueScopeFromName(const FBIMNameType &InName)
{
	return EnumValueByString(EBIMValueScope, InName.ToString());
}

FBIMNameType FBIMPropertyValue::QN() const
{
	return *FString::Printf(TEXT("%s.%s"), *BIMNameFromValueScope(Scope).ToString(),*(Name.ToString()));
}

FBIMPropertyValue::FValue FBIMPropertySheet::GetProperty(EBIMValueScope InScope, const FBIMNameType &InName) const
{
	return FModumateFunctionParameterSet::GetValue(FBIMPropertyValue(InScope, EBIMValueType::None, InName).QN().ToString());
}

void FBIMPropertySheet::SetProperty(EBIMValueScope InScope, const FBIMNameType &InName, const FBIMPropertyValue::FValue &InParam)
{
	FBIMNameType QN = FBIMPropertyValue(InScope, EBIMValueType::None, InName).QN();
	FModumateFunctionParameterSet::SetValue(QN.ToString(), InParam);
	TArray<FBIMNameType> *boundProps = PropertyBindings.Find(QN);
	if (boundProps != nullptr)
	{
		for (auto &prop : *boundProps)
		{
			FBIMPropertyValue vs(prop);
			FModumateFunctionParameterSet::SetValue(vs.QN().ToString(), InParam);
		}
	}
}

bool FBIMPropertySheet::HasProperty(EBIMValueScope InScope, const FBIMNameType &InName) const
{
	return FModumateFunctionParameterSet::HasValue(FBIMPropertyValue(InScope, EBIMValueType::None, InName).QN().ToString());
}

void FBIMPropertySheet::RemoveProperty(EBIMValueScope InScope, const FBIMNameType &InName)
{
	FBIMNameType QN = FBIMPropertyValue(InScope, EBIMValueType::None, InName).QN();
	PropertyBindings.Remove(QN);
	FModumateFunctionParameterSet::Remove(QN.ToString());
}

bool FBIMPropertySheet::BindProperty(EBIMValueScope SourceScope, const FBIMNameType &SourceName, EBIMValueScope TargetScope, const FBIMNameType &TargetName)
{
	// Bound properties cannot pre-exist with their own values
	if (HasProperty(TargetScope, TargetName))
	{
		return false;
	}

	// The source for a bound property must exist
	if (!HasProperty(SourceScope, SourceName))
	{
		return false;
	}

	SetProperty(TargetScope, TargetName, GetProperty(SourceScope,SourceName));
			
	FBIMPropertyValue source(SourceScope, EBIMValueType::None, SourceName);
	FBIMPropertyValue target(TargetScope, EBIMValueType::None, TargetName);

	TArray<FBIMNameType> &targets = PropertyBindings.FindOrAdd(source.QN());
	if (!targets.Contains(target.QN()))
	{
		targets.Add(target.QN());
		return true;
	}
	return false;
}

bool FBIMPropertySheet::FromDataRecord(const FBIMPropertySheetRecord &InRecord)
{
	Empty();
	PropertyBindings.Empty();

	if (!ensureAlways(InRecord.BindingSources.Num() == InRecord.BindingTargets.Num()))
	{
		return false;
	}

	FromStringMap(InRecord.Properties);

	for (int32 i = 0; i < InRecord.BindingSources.Num(); ++i)
	{
		FBIMPropertyValue source(InRecord.BindingSources[i]);
		FBIMPropertyValue target(InRecord.BindingTargets[i]);
		TArray<FBIMNameType> &targets = PropertyBindings.FindOrAdd(source.QN());
		targets.Add(target.QN());
	}

	return true;
}

bool FBIMPropertySheet::ToDataRecord(FBIMPropertySheetRecord &OutRecord) const
{
	ToStringMap(OutRecord.Properties);
	for (auto &sourceProp : PropertyBindings)
	{
		for (auto &targetProp : sourceProp.Value)
		{
			OutRecord.BindingSources.Add(sourceProp.Key);
			OutRecord.BindingTargets.Add(targetProp);
		}
	}
	return true;
}

// Todo: re-evaluate as an enum when variable catalog stabilizes
namespace BIMPropertyNames
{
	const FBIMNameType Angle = TEXT("Angle");
	const FBIMNameType Area = TEXT("Area");
	const FBIMNameType AreaType = TEXT("AreaType");
	const FBIMNameType AssetID = TEXT("AssetID");
	const FBIMNameType AssetPath = TEXT("AssetPath");
	const FBIMNameType BevelWidth = TEXT("BevelWidth");
	const FBIMNameType Category = TEXT("Category");
	const FBIMNameType Code = TEXT("Code");
	const FBIMNameType Color = TEXT("Color");
	const FBIMNameType Comments = TEXT("Comments");
	const FBIMNameType Configuration = TEXT("Configuration");
	const FBIMNameType Depth = TEXT("Depth");
	const FBIMNameType Diameter = TEXT("Diameter");
	const FBIMNameType DimensionSetComments = TEXT("DimensionSetComments");
	const FBIMNameType DimensionSetName = TEXT("DimensionSetName");
	const FBIMNameType Dimensions = TEXT("Dimensions");
	const FBIMNameType EngineMaterial = TEXT("EngineMaterial");
	const FBIMNameType Extents = TEXT("Extents");
	const FBIMNameType Form = TEXT("Form");
	const FBIMNameType Function = TEXT("Function");
	const FBIMNameType HasFace = TEXT("HasFace");
	const FBIMNameType Height = TEXT("Height");
	const FBIMNameType HexValue = TEXT("HexValue");
	const FBIMNameType Length = TEXT("Length");
	const FBIMNameType LoadFactorSpecialCalc = TEXT("LoadFactorSpecialCalc");
	const FBIMNameType MaterialKey = TEXT("MaterialKey");
	const FBIMNameType MaterialColor = TEXT("MaterialColor");
	const FBIMNameType Mesh = TEXT("Mesh");
	const FBIMNameType ModuleCount = TEXT("ModuleCount");
	const FBIMNameType Name = TEXT("Name");
	const FBIMNameType NamedDimensions = TEXT("NamedDimensions");
	const FBIMNameType Normal = TEXT("Normal");
	const FBIMNameType Number = TEXT("Number");
	const FBIMNameType OccupantLoadFactor = TEXT("OccupantLoadFactor");
	const FBIMNameType OccupantsNumber = TEXT("OccupantsNumber");
	const FBIMNameType Overhang = TEXT("Overhang");
	const FBIMNameType Override = TEXT("Override");
	const FBIMNameType PartSet = TEXT("PartSet");
	const FBIMNameType Pattern = TEXT("Pattern");
	const FBIMNameType Preset = TEXT("Preset");
	const FBIMNameType ProfileKey = TEXT("ProfileKey");
	const FBIMNameType Scale = TEXT("Scale");
	const FBIMNameType Slope = TEXT("Slope");
	const FBIMNameType StyleComments = TEXT("StyleComments");
	const FBIMNameType StyleName = TEXT("StyleName");
	const FBIMNameType Subcategory = TEXT("Subcategory");
	const FBIMNameType Tag = TEXT("Tag");
	const FBIMNameType Tangent = TEXT("Tangent");
	const FBIMNameType Thickness = TEXT("Thickness");
	const FBIMNameType Title = TEXT("Title");
	const FBIMNameType ToeKickDepth = TEXT("ToeKickDepth");
	const FBIMNameType ToeKickHeight = TEXT("ToeKickHeight");
	const FBIMNameType TrimProfile = TEXT("TrimProfile");
	const FBIMNameType UseGroupType = TEXT("UseGroupType");
	const FBIMNameType Width = TEXT("Width");
}
