// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Core/BIMProperties.h"
#include "DocumentManagement/ModumateSerialization.h"

struct MODUMATE_API FBIMPropertySheetRecord;

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

FBIMPropertyKey::FBIMPropertyKey(EBIMValueScope InScope, const FBIMNameType& InName) :
	Scope(InScope),
	Name(InName)
{}

FBIMPropertyKey::FBIMPropertyKey(const FBIMNameType& InQN)
{
	TArray<FString> parts;
	InQN.ToString().ParseIntoArray(parts, TEXT("."));
	if (ensureAlways(parts.Num() == 2))
	{
		Scope = BIMValueScopeFromName(*parts[0]);
		Name = *parts[1];
	}
}

FBIMNameType FBIMPropertyKey::QN() const
{
	return *FString::Printf(TEXT("%s.%s"), *BIMNameFromValueScope(Scope).ToString(),*(Name.ToString()));
}

bool FBIMPropertySheet::Matches(const FBIMPropertySheet& PropSheet) const
{
	if (StringMap.Num() != PropSheet.StringMap.Num())
	{
		return false;
	}
	if (NumberMap.Num() != PropSheet.NumberMap.Num())
	{
		return false;
	}

	for (auto& kvp : StringMap)
	{
		const FString* theirString = PropSheet.StringMap.Find(kvp.Key);
		if (theirString == nullptr || !theirString->Equals(kvp.Value))
		{
			return false;
		}
	}

	for (auto& kvp : NumberMap)
	{
		const float* theirNum = PropSheet.NumberMap.Find(kvp.Key);
		// Properties may come in from a database or a saved document file, may be an epsilon
		if (theirNum == nullptr || !FMath::IsNearlyEqual(kvp.Value, *theirNum))
		{
			return false;
		}
	}

	return true;
}

EBIMResult FBIMPropertySheet::AddProperties(const FBIMPropertySheet& PropSheet)
{
	for (auto& kvp : PropSheet.StringMap)
	{
		StringMap.Add(kvp.Key, kvp.Value);
	}
	for (auto& kvp : PropSheet.NumberMap)
	{
		NumberMap.Add(kvp.Key, kvp.Value);
	}
	return EBIMResult::Success;
}

bool FBIMPropertySheet::TryGetProperty(EBIMValueScope InScope, const FBIMNameType& InName, FName& OutT) const
{
	FString stringKey;
	if (TryGetProperty<FString>(InScope, InName, stringKey))
	{
		OutT = *stringKey;
		return true;
	}
	return false;
}

bool FBIMPropertySheet::TryGetProperty(EBIMValueScope InScope, const FBIMNameType& InName, FBIMKey& OutT) const
{
	FString stringKey;
	if (TryGetProperty<FString>(InScope, InName, stringKey))
	{
		OutT = FBIMKey(stringKey);
		return true;
	}
	return false;
}

bool FBIMPropertySheet::TryGetProperty(EBIMValueScope InScope, const FBIMNameType& InName, FText& OutT) const
{
	FString stringKey;
	if (TryGetProperty<FString>(InScope, InName, stringKey))
	{
		OutT = FText::FromString(stringKey);
		return true;
	}
	return false;
}

bool FBIMPropertySheet::TryGetProperty(EBIMValueScope InScope, const FBIMNameType& InName, Modumate::Units::FUnitValue& OutT) const
{
	float v;
	if (TryGetProperty<float>(InScope, InName, v))
	{
		OutT = Modumate::Units::FUnitValue::WorldCentimeters(v);
		return true;
	}
	return false;
}

bool FBIMPropertySheet::TryGetProperty(EBIMValueScope InScope, const FBIMNameType& InName, int32& OutT) const
{
	float v;
	if (TryGetProperty<float>(InScope, InName, v))
	{
		OutT = static_cast<int32>(v);
		return true;
	}
	return false;
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
	const FBIMNameType ConceptualSizeY = TEXT("ConceptualSizeY");
	const FBIMNameType Configuration = TEXT("Configuration");
	const FBIMNameType CraftingIconAssetFilePath = TEXT("CraftingIconAssetFilePath");
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
	const FBIMNameType ID = TEXT("ID");
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
	const FBIMNameType Recess = TEXT("Recess");
	const FBIMNameType Scale = TEXT("Scale");
	const FBIMNameType Slope = TEXT("Slope");
	const FBIMNameType StyleComments = TEXT("StyleComments");
	const FBIMNameType StyleName = TEXT("StyleName");
	const FBIMNameType Subcategory = TEXT("Subcategory");
	const FBIMNameType SupportedNCPs = TEXT("SupportedNCPs");
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
