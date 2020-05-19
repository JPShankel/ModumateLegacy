// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateStats.h"
#include "Runtime/JsonUtilities/Public/JsonObjectConverter.h"

namespace Modumate
{
	TArray<FVector> FModumateCommandParameter::AsVectorArray() const
	{
		FModumateVectorArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateVectorArrayParam>(StringValue, &ret,0,0);
		return ret.Values;
	}

	TArray<FVector2D> FModumateCommandParameter::AsVector2DArray() const
	{
		FModumateVector2DArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateVector2DArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	TArray<FRotator> FModumateCommandParameter::AsRotatorArray() const
	{
		FModumateRotatorArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateRotatorArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	TArray<FQuat> FModumateCommandParameter::AsQuatArray() const
	{
		FModumateQuatArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateQuatArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	TArray<int32> FModumateCommandParameter::AsIntArray() const
	{
		FModumateIntArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateIntArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	TArray<float> FModumateCommandParameter::AsFloatArray() const
	{
		FModumateFloatArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateFloatArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	TArray<FString> FModumateCommandParameter::AsStringArray() const
	{
		FModumateStringArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateStringArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	TArray<FName> FModumateCommandParameter::AsNameArray() const
	{
		FModumateNameArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateNameArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	TArray<bool> FModumateCommandParameter::AsBoolArray() const
	{
		FModumateBoolArrayParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateBoolArrayParam>(StringValue, &ret, 0, 0);
		return ret.Values;
	}

	FVector FModumateCommandParameter::AsVector() const
	{
		FModumateFVectorParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateFVectorParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	FVector2D FModumateCommandParameter::AsVector2D() const
	{
		FModumateFVector2DParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateFVector2DParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	FRotator FModumateCommandParameter::AsRotator() const
	{
		FModumateFRotatorParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateFRotatorParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	FQuat FModumateCommandParameter::AsQuat() const
	{
		FModumateFQuatParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateFQuatParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	float FModumateCommandParameter::AsFloat() const
	{
		FModumateFloatParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateFloatParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	int32 FModumateCommandParameter::AsInt() const
	{
		FModumateIntParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateIntParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	FString FModumateCommandParameter::AsString() const
	{
		FModumateStringParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateStringParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	FName FModumateCommandParameter::AsName() const
	{
		FModumateNameParam ret;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateNameParam>(StringValue, &ret, 0, 0);
		return ret.Value;
	}

	bool FModumateCommandParameter::AsBool() const
	{
		return AsString() == TEXT("true");
	}

	void FModumateCommandParameter::FromVector(const FVector &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateFVectorParam>(FModumateFVectorParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromVector2D(const FVector2D &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateFVector2DParam>(FModumateFVector2DParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromRotator(const FRotator &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateFRotatorParam>(FModumateFRotatorParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromQuat(const FQuat &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateFQuatParam>(FModumateFQuatParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromFloat(float v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateFloatParam>(FModumateFloatParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromInt(int32 i)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateIntParam>(FModumateIntParam(i), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromBool(bool b)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateStringParam>(FModumateStringParam(b ? TEXT("true") : TEXT("false")), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromString(const FString &s)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateStringParam>(FModumateStringParam(s), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromName(const FName &s)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateNameParam>(FModumateNameParam(s), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromVectorArray(const TArray<FVector> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateVectorArrayParam>(FModumateVectorArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromVector2DArray(const TArray<FVector2D> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateVector2DArrayParam>(FModumateVector2DArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromRotatorArray(const TArray<FRotator> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateRotatorArrayParam>(FModumateRotatorArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromQuatArray(const TArray<FQuat> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateQuatArrayParam>(FModumateQuatArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromIntArray(const TArray<int32> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateIntArrayParam>(FModumateIntArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromFloatArray(const TArray<float> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateFloatArrayParam>(FModumateFloatArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromStringArray(const TArray<FString> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateStringArrayParam>(FModumateStringArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromNameArray(const TArray<FName> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateNameArrayParam>(FModumateNameArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	void FModumateCommandParameter::FromBoolArray(const TArray<bool> &v)
	{
		FJsonObjectConverter::UStructToJsonObjectString<FModumateBoolArrayParam>(FModumateBoolArrayParam(v), StringValue, 0, 0, 0, nullptr, false);
	}

	FString FModumateCommandParameter::AsJSON() const
	{
		return StringValue;
	}

	void FModumateCommandParameter::FromJSON(const FString &json)
	{
		StringValue = json;
	}

	int32 FModumateCommand::NextCommandID = 1;
	const FString FModumateCommand::VersionString(TEXT("version"));
	const FString FModumateCommand::CommandIDString(TEXT("commandID"));
	const FString FModumateCommand::CommandFieldString(TEXT("command"));

	FModumateCommand::FModumateCommand(const FString &command, bool bInCaptureForInput, int32 version)
	{
		CachedCommandID = NextCommandID++;
		bCaptureForInput = bInCaptureForInput;
		Param(FModumateCommand::CommandFieldString, command);
		Param(FModumateCommand::VersionString, FString::FromInt(version));
		Param(FModumateCommand::CommandIDString, FString::FromInt(CachedCommandID));
	}

	FModumateCommand &FModumateCommand::Param(const FModumateCommandParameter &param)
	{
		JSONValue.Parameters.Add(param.ParameterName, param.StringValue);
		return *this;
	}

	DECLARE_CYCLE_STAT(TEXT("Command to JSON"), STAT_CommandToJSON, STATGROUP_Modumate)

	FString FModumateCommand::GetJSONString() const
	{
		SCOPE_CYCLE_COUNTER(STAT_CommandToJSON);
		FString jsonString;
		FJsonObjectConverter::UStructToJsonObjectString<FModumateCommandJSON>(JSONValue, jsonString,0,0,0,nullptr,false);
		return jsonString;
	}

	FModumateFunctionParameterSet FModumateCommand::GetParameterSet() const
	{
		FModumateFunctionParameterSet ret;
		for (auto &kvp : JSONValue.Parameters)
		{
			ret.Add(kvp.Key, kvp.Value);
		}
		return ret;
	}

	void FModumateCommand::SetParameterSet(const FModumateFunctionParameterSet &params)
	{
		params.ForEachProperty([this](const FString &name, const FModumateCommandParameter &param)
		{
			JSONValue.Parameters.Add(param.ParameterName, param.StringValue);
		});
	}

	bool FModumateFunctionParameterSet::HasValue(const FString &key) const
	{
		return Contains(key);
	}

	DECLARE_CYCLE_STAT(TEXT("Command from JSON"), STAT_CommandFromJSON, STATGROUP_Modumate)

	FModumateCommand FModumateCommand::FromJSONString(const FString &jsonString)
	{
		SCOPE_CYCLE_COUNTER(STAT_CommandFromJSON);
		FModumateCommand cmd;
		FModumateCommandJSON val;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateCommandJSON>(jsonString, &cmd.JSONValue,0,0);

		FString *cmdIDstr = cmd.JSONValue.Parameters.Find(FModumateCommand::CommandIDString);
		if (ensureAlways(cmdIDstr != nullptr))
		{
			FString str = *cmdIDstr;
			cmd.CachedCommandID = FCString::Atoi(*str);
		}
		return cmd;
	}

	FModumateCommandParameter FModumateFunctionParameterSet::GetValue(const FString &key, const FModumateCommandParameter &defaultValue) const
	{
		const FString *val = Find(key);
		if (val == nullptr)
		{
			return defaultValue;
		}
		FModumateCommandParameter ret;
		ret.StringValue = *val;
		ret.ParameterName = key;
		return ret;
	}

	FModumateFunctionParameterSet &FModumateFunctionParameterSet::SetValue(const FString &key, const FModumateCommandParameter &val)
	{
		Add(key, val.StringValue);
		return *this;
	}

	void FModumateFunctionParameterSet::GetValueNames(TArray<FString> &outNames) const
	{
		GenerateKeyArray(outNames);
	}

	void FModumateFunctionParameterSet::ForEachProperty(std::function<void(const FString &name, const FModumateCommandParameter &mcp)> fn) const
	{
		for (auto &kvp : *this)
		{
			fn(kvp.Key, GetValue(kvp.Key));
		}
	}

	void FModumateFunctionParameterSet::Empty()
	{
		FPrivateBaseClass::Empty();
	}

	void FModumateFunctionParameterSet::Remove(const FString &key)
	{
		FPrivateBaseClass::Remove(key);
	}

	int32 FModumateFunctionParameterSet::Num() const
	{
		return FPrivateBaseClass::Num();
	}


	bool FModumateFunctionParameterSet::Matches(const FModumateFunctionParameterSet &MatchSet) const
	{
		if (MatchSet.Num() != Num())
		{
			return false;
		}

		for (auto &kvp : *this)
		{
			const FString *pVal = MatchSet.Find(kvp.Key);
			if (pVal == nullptr || *pVal != kvp.Value)
			{
				return false;
			}
		}

		for (auto &kvp : MatchSet)
		{
			const FString *pVal = Find(kvp.Key);
			if (pVal == nullptr || *pVal != kvp.Value)
			{
				return false;
			}
		}

		return true;
	}


	/*
	String map functions take advantage of parameter set's private inheritance from TMap<FString,FString>
	They'll have to be refactored if inheritance changes
	*/
	bool FModumateFunctionParameterSet::ToStringMap(FModumateFunctionParameterSet::FStringMap &OutMap) const
	{
		OutMap = static_cast<FStringMap>(*this);
		return true;
	}

	bool FModumateFunctionParameterSet::FromStringMap(const FModumateFunctionParameterSet::FStringMap &Map)
	{
		Empty();
		for (auto &kvp : Map)
		{
			Add(kvp);
		}
		return true;
	}
}
