// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMProperties.generated.h"

struct MODUMATE_API FBIMPropertySheetRecord;

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
	Error = 255
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
	Roof,
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
	Error = 255
};

UENUM(BlueprintType)
enum class ECraftingNodePresetStatus : uint8
{
	None = 0,
	UpToDate,
	Dirty,
	Pending
};

namespace Modumate {
	namespace BIM {

		typedef FName FNameType;
		typedef FModumateCommandParameter FValue;

		typedef EBIMValueType EValueType;
		typedef EBIMValueScope EScope;

		FNameType NameFromValueType(EValueType vt);
		EValueType ValueTypeFromName(const FNameType &name);

		FNameType NameFromScope(EScope s);
		EScope ScopeFromName(const FNameType &name);

		struct MODUMATE_API FValueSpec
		{
			FValueSpec() : Scope(EScope::None), Type(EValueType::None), Value(0) {}

			FValueSpec(EScope InScope, EValueType InType, const FNameType &InName);
			FValueSpec(EScope InScope, EValueType InType, const FNameType &InName, const FValue &InValue);

			FValueSpec(EValueType InType, const FNameType &InQN);
			FValueSpec(EValueType InType, const FNameType &InQN, const FValue &InValue);

			FValueSpec(const FNameType &InQN);
			FValueSpec(const FNameType &InQN, const FValue &InValue);

			FNameType QN() const;

			EScope Scope;
			EValueType Type;
			FNameType Name;
			FValue Value;
		};

		// TODO: Re-evaulate as an enum if/when variable list stabilizes and no custom variables are required
		namespace Parameters
		{
			extern const FNameType Angle;
			extern const FNameType Area;
			extern const FNameType AreaType;
			extern const FNameType AssetID;
			extern const FNameType BevelWidth;
			extern const FNameType Category;
			extern const FNameType Code;
			extern const FNameType Color;
			extern const FNameType Comments;
			extern const FNameType Configuration;
			extern const FNameType Depth;
			extern const FNameType Diameter;
			extern const FNameType DimensionSetComments;
			extern const FNameType DimensionSetName;
			extern const FNameType Dimensions;
			extern const FNameType EngineMaterial;
			extern const FNameType Extents;
			extern const FNameType Form;
			extern const FNameType Function;
			extern const FNameType HasFace;
			extern const FNameType Height;
			extern const FNameType HexValue;
			extern const FNameType Length;
			extern const FNameType LoadFactorSpecialCalc;
			extern const FNameType MaterialKey;
			extern const FNameType MaterialColor;
			extern const FNameType Mesh;
			extern const FNameType ModuleCount;
			extern const FNameType Name;
			extern const FNameType Normal;
			extern const FNameType Number;
			extern const FNameType OccupantLoadFactor;
			extern const FNameType OccupantsNumber;
			extern const FNameType Overhang;
			extern const FNameType Override;
			extern const FNameType PartSet;
			extern const FNameType Pattern;
			extern const FNameType Preset;
			extern const FNameType ProfileKey;
			extern const FNameType Scale;
			extern const FNameType Slope;
			extern const FNameType StyleComments;
			extern const FNameType StyleName;
			extern const FNameType Subcategory;
			extern const FNameType Tag;
			extern const FNameType Tangent;
			extern const FNameType Thickness;
			extern const FNameType Title;
			extern const FNameType TrimProfile;
			extern const FNameType UseGroupType;
			extern const FNameType Width;
			extern const FNameType XExtents;
			extern const FNameType YExtents;
			extern const FNameType ZExtents;
		}

		class MODUMATE_API FBIMPropertySheet : public FModumateFunctionParameterSet
		{
		private:
			TMap<FNameType, TArray<FNameType>> PropertyBindings;
		public:
			// Scope-aware 'Property' set/get helper functions
			// Use get/set value in parent for qualified variable strings (ie 'Module.Count')
			FValue GetProperty(EScope Scope, const FNameType &Name) const;
			void SetProperty(EScope Scope, const FNameType &Name, const FValue &Param);
			bool HasProperty(EScope Scope, const FNameType &Name) const;

			void RemoveProperty(EScope Scope, const FNameType &Name);

			// When a target property is bound to a source, it is updated with the value of the source
			bool BindProperty(EScope SourceScope, const FNameType &SourceName, EScope TargetScope, const FNameType &TargetName);

			template<class T>
			bool TryGetProperty(EScope Scope, const FNameType &Name, T &OutT) const
			{
				if (HasProperty(Scope, Name))
				{
					OutT = GetProperty(Scope, Name);
					return true;
				}
				return false;
			}

			bool FromDataRecord(const FBIMPropertySheetRecord &InRecord);
			bool ToDataRecord(FBIMPropertySheetRecord &OutRecord) const;
		};
	}
}
