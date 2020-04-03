// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateObjectEnums.h"
#include "ModumateConsoleCommand.h"


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
			extern const FNameType Extents;
			extern const FNameType Form;
			extern const FNameType Function;
			extern const FNameType Height;
			extern const FNameType HexValue;
			extern const FNameType Length;
			extern const FNameType LoadFactorSpecialCalc;
			extern const FNameType Material;
			extern const FNameType MaterialColor;
			extern const FNameType Mesh;
			extern const FNameType ModuleCount;
			extern const FNameType Name;
			extern const FNameType Normal;
			extern const FNameType Number;
			extern const FNameType OccupantLoadFactor;
			extern const FNameType OccupantsNumber;
			extern const FNameType PartSet;
			extern const FNameType Pattern;
			extern const FNameType Preset;
			extern const FNameType Scale;
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
		public:
			// Scope-aware 'Property' set/get helper functions
			// Use get/set value in parent for qualified variable strings (ie 'Module.Count')
			FValue GetProperty(EScope scope, const FNameType &name) const;
			void SetProperty(EScope scope, const FNameType &name, const FValue &param);
			bool HasProperty(EScope scope, const FNameType &name) const;

			void RemoveProperty(EScope scope, const FNameType &name);

			template<class T>
			bool TryGetProperty(EScope scope, const FNameType &name, T &outT) const
			{
				if (HasProperty(scope, name))
				{
					outT = GetProperty(scope, name);
					return true;
				}
				return false;
			}
		};
	}
}
