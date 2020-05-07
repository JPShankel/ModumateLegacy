// Copyright 2019 Modumate, Inc. All Rights Reserved.


#include "ModumateBIMSchema.h"
#include "ModumateSerialization.h"

struct MODUMATE_API FBIMPropertySheetRecord;

namespace Modumate {
	namespace BIM {

		FValueSpec::FValueSpec(EScope InScope, EValueType InType, const FNameType &InName) :
			Scope(InScope),
			Type(InType),
			Name(InName),
			Value(0)
		{}

		FValueSpec::FValueSpec(EScope InScope, EValueType InType, const FNameType &InName, const FValue &InValue) :
			Scope(InScope),
			Type(InType),
			Name(InName),
			Value(InValue)
		{}

		FValueSpec::FValueSpec(EValueType InType, const FNameType &InQN) : Type(InType), Value(0)
		{
			if (InQN.IsNone())
			{
				Scope = EScope::None;
				Name = NAME_None;
			}
			else
			{
				TArray<FString> split;
				InQN.ToString().ParseIntoArray(split, TEXT("."));
				if (ensureAlways(split.Num() == 2))
				{
					Scope = ScopeFromName(*split[0]);
					Name = *split[1];
				}
			}
		}

		FValueSpec::FValueSpec(EValueType InType, const FNameType &InQN, const FValue &InValue) : FValueSpec(InType,InQN)
		{
			Value = InValue;
		}

		FValueSpec::FValueSpec(const FNameType &InQN) : FValueSpec(EValueType::None, InQN)
		{}

		FValueSpec::FValueSpec(const FNameType &InQN, const FValue &InValue) : FValueSpec(EValueType::None, InQN, InValue)
		{}

		FNameType NameFromValueType(EValueType InValueType)
		{
			return *EnumValueString(EBIMValueType, InValueType);
		}

		EValueType ValueTypeFromName(const FNameType &InName)
		{
			return EnumValueByString(EBIMValueType, InName.ToString());
		}

		FNameType NameFromScope(EScope InScope)
		{
			return *EnumValueString(EBIMValueScope, InScope);
		}

		EScope ScopeFromName(const FNameType &InName)
		{
			return EnumValueByString(EBIMValueScope, InName.ToString());
		}

		FNameType FValueSpec::QN() const
		{
			return *FString::Printf(TEXT("%s.%s"), *NameFromScope(Scope).ToString(),*(Name.ToString()));
		}

		FValue FBIMPropertySheet::GetProperty(EScope InScope, const FNameType &InName) const
		{
			return FModumateFunctionParameterSet::GetValue(FValueSpec(InScope, EValueType::None, InName).QN().ToString());
		}

		void FBIMPropertySheet::SetProperty(EScope InScope, const FNameType &InName, const FValue &InParam)
		{
			FNameType QN = FValueSpec(InScope, EValueType::None, InName).QN();
			FModumateFunctionParameterSet::SetValue(QN.ToString(), InParam);
			TArray<FNameType> *boundProps = PropertyBindings.Find(QN);
			if (boundProps != nullptr)
			{
				for (auto &prop : *boundProps)
				{
					FValueSpec vs(prop);
					FModumateFunctionParameterSet::SetValue(vs.QN().ToString(), InParam);
				}
			}
		}

		bool FBIMPropertySheet::HasProperty(EScope InScope, const FNameType &InName) const
		{
			return FModumateFunctionParameterSet::HasValue(FValueSpec(InScope, EValueType::None, InName).QN().ToString());
		}

		void FBIMPropertySheet::RemoveProperty(EScope InScope, const FNameType &InName)
		{
			FNameType QN = FValueSpec(InScope, EValueType::None, InName).QN();
			PropertyBindings.Remove(QN);
			FModumateFunctionParameterSet::Remove(QN.ToString());
		}

		bool FBIMPropertySheet::BindProperty(EScope SourceScope, const FNameType &SourceName, EScope TargetScope, const FNameType &TargetName)
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
			
			FValueSpec source(SourceScope, EValueType::None, SourceName);
			FValueSpec target(TargetScope, EValueType::None, TargetName);

			TArray<FNameType> &targets = PropertyBindings.FindOrAdd(source.QN());
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
				FValueSpec source(InRecord.BindingSources[i]);
				FValueSpec target(InRecord.BindingTargets[i]);
				TArray<FNameType> &targets = PropertyBindings.FindOrAdd(source.QN());
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
		namespace Parameters
		{
			const FNameType Angle = TEXT("Angle");
			const FNameType Area = TEXT("Area");
			const FNameType AreaType = TEXT("AreaType");
			const FNameType AssetID = TEXT("AssetID");
			const FNameType BevelWidth = TEXT("BevelWidth");
			const FNameType Category = TEXT("Category");
			const FNameType Code = TEXT("Code");
			const FNameType Color = TEXT("Color");
			const FNameType Comments = TEXT("Comments");
			const FNameType Configuration = TEXT("Configuration");
			const FNameType Depth = TEXT("Depth");
			const FNameType Diameter = TEXT("Diameter");
			const FNameType DimensionSetComments = TEXT("DimensionSetComments");
			const FNameType DimensionSetName = TEXT("DimensionSetName");
			const FNameType Dimensions = TEXT("Dimensions");
			const FNameType Extents = TEXT("Extents");
			const FNameType Form = TEXT("Form");
			const FNameType Function = TEXT("Function");
			const FNameType Height = TEXT("Height");
			const FNameType HexValue = TEXT("HexValue");
			const FNameType Length = TEXT("Length");
			const FNameType LoadFactorSpecialCalc = TEXT("LoadFactorSpecialCalc");
			const FNameType Material = TEXT("Material");
			const FNameType MaterialColor = TEXT("MaterialColor");
			const FNameType Mesh = TEXT("Mesh");
			const FNameType ModuleCount = TEXT("ModuleCount");
			const FNameType Name = TEXT("Name");
			const FNameType Normal = TEXT("Normal");
			const FNameType Number = TEXT("Number");
			const FNameType OccupantLoadFactor = TEXT("OccupantLoadFactor");
			const FNameType OccupantsNumber = TEXT("OccupantsNumber");
			const FNameType PartSet = TEXT("PartSet");
			const FNameType Pattern = TEXT("Pattern");
			const FNameType Preset = TEXT("Preset");
			const FNameType Scale = TEXT("Scale");
			const FNameType StyleComments = TEXT("StyleComments");
			const FNameType StyleName = TEXT("StyleName");
			const FNameType Subcategory = TEXT("Subcategory");
			const FNameType Tag = TEXT("Tag");
			const FNameType Tangent = TEXT("Tangent");
			const FNameType Thickness = TEXT("Thickness");
			const FNameType Title = TEXT("Title");
			const FNameType TrimProfile = TEXT("TrimProfile");
			const FNameType UseGroupType = TEXT("UseGroupType");
			const FNameType Width = TEXT("Width");
			const FNameType XExtents = TEXT("XExtents");
			const FNameType YExtents = TEXT("YExtents");;
			const FNameType ZExtents = TEXT("ZExtents");;
		}
	}
}
