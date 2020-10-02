// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MOIState.h"

// TODO: remove these
#include "DocumentManagement/ModumateCommands.h"


bool FMOIStateData_DEPRECATED::ToParameterSet(const FString& Prefix, Modumate::FModumateFunctionParameterSet& OutParameterSet) const
{
	TMap<FString, FString> propertyMap;
	if (!ObjectProperties.ToStringMap(propertyMap))
	{
		return false;
	}

	TArray<FString> propertyNames, propertyValues;

	propertyMap.GenerateKeyArray(propertyNames);
	propertyMap.GenerateValueArray(propertyValues);

	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kPropertyNames, propertyNames);
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kPropertyValues, propertyValues);

	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kExtents, Extents);
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kIndices, ControlIndices);
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kInverted, bObjectInverted);
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kAssembly, ObjectAssemblyKey.ToString());
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kParent, ParentID);
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kLocation, Location);
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kQuaternion, Orientation);
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kType, EnumValueString(EMOIDeltaType, StateType));
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kObjectType, EnumValueString(EObjectType, ObjectType));
	OutParameterSet.SetValue(Prefix + Modumate::Parameters::kObjectID, ObjectID);

	return true;
}

bool FMOIStateData_DEPRECATED::FromParameterSet(const FString& Prefix, const Modumate::FModumateFunctionParameterSet& ParameterSet)
{
	TArray<FString> propertyNames, propertyValues;
	propertyNames = ParameterSet.GetValue(Prefix + Modumate::Parameters::kPropertyNames, propertyNames);
	propertyValues = ParameterSet.GetValue(Prefix + Modumate::Parameters::kPropertyValues, propertyValues);

	if (!ensureAlways(propertyNames.Num() == propertyValues.Num()))
	{
		return false;
	}

	Modumate::FModumateFunctionParameterSet::FStringMap propertyMap;
	for (int32 i = 0; propertyNames.Num(); ++i)
	{
		propertyMap.Add(propertyNames[i], propertyValues[i]);
	}

	ObjectProperties.FromStringMap(propertyMap);

	Extents = ParameterSet.GetValue(Prefix + Modumate::Parameters::kExtents);
	ControlIndices = ParameterSet.GetValue(Prefix + Modumate::Parameters::kIndices);
	bObjectInverted = ParameterSet.GetValue(Prefix + Modumate::Parameters::kInverted);
	ObjectAssemblyKey = FBIMKey(ParameterSet.GetValue(Prefix + Modumate::Parameters::kAssembly).AsString());
	ParentID = ParameterSet.GetValue(Prefix + Modumate::Parameters::kParent);
	Location = ParameterSet.GetValue(Prefix + Modumate::Parameters::kLocation);
	Orientation = ParameterSet.GetValue(Prefix + Modumate::Parameters::kQuaternion);
	StateType = EnumValueByString(EMOIDeltaType, ParameterSet.GetValue(Prefix + Modumate::Parameters::kType).AsString());
	ObjectID = ParameterSet.GetValue(Prefix + Modumate::Parameters::kObjectID);
	ObjectType = EnumValueByString(EObjectType, ParameterSet.GetValue(Prefix + Modumate::Parameters::kObjectType).AsString());

	return true;
}

bool FMOIStateData_DEPRECATED::operator==(const FMOIStateData_DEPRECATED& Other) const
{
	// TODO: replace with better per-property equality checks, when we no longer use FModumateFunctionParameterSet for serialization.
	Modumate::FModumateFunctionParameterSet thisParamSet, otherParamSet;
	TMap<FString, FString> thisStringMap, otherStringMap;
	if (ToParameterSet(TEXT(""), thisParamSet) && Other.ToParameterSet(TEXT(""), otherParamSet) &&
		thisParamSet.ToStringMap(thisStringMap) && otherParamSet.ToStringMap(otherStringMap))
	{
		return thisStringMap.OrderIndependentCompareEqual(otherStringMap);
	}

	return false;
}

FMOIStateData::FMOIStateData()
{
}

FMOIStateData::FMOIStateData(int32 InID, EObjectType InObjectType, int32 InParentID)
	: ID(InID)
	, ObjectType(InObjectType)
	, ParentID(InParentID)
{
}

bool FMOIStateData::operator==(const FMOIStateData& Other) const
{
	return (ID == Other.ID) &&
		(ObjectType == Other.ObjectType) &&
		(ParentID == Other.ParentID) &&
		(AssemblyKey == Other.AssemblyKey) &&
		(CustomData == Other.CustomData);
}

bool FMOIStateData::operator!=(const FMOIStateData& Other) const
{
	return !(*this == Other);
}
