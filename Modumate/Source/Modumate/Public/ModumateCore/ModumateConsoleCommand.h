// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include <functional>
#include "ModumateConsoleCommand.generated.h"

USTRUCT()
struct FModumateCommandJSON
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FString> Parameters;
};

USTRUCT()
struct FModumateFVectorParam
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Value = FVector::ZeroVector;

	FModumateFVectorParam() {};
	FModumateFVectorParam(const FVector& v) : Value(v) {}
};

USTRUCT()
struct FModumateFVector2DParam
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Value = FVector2D::ZeroVector;

	FModumateFVector2DParam() {}
	FModumateFVector2DParam(const FVector2D& v) : Value(v) {}
};

USTRUCT()
struct FModumateFRotatorParam
{
	GENERATED_BODY()

	UPROPERTY()
	FRotator Value = FRotator::ZeroRotator;

	FModumateFRotatorParam() {}
	FModumateFRotatorParam(const FRotator& v) : Value(v) {}
};

USTRUCT()
struct FModumateFQuatParam
{
	GENERATED_BODY()

	UPROPERTY()
	FQuat Value = FQuat::Identity;

	FModumateFQuatParam() {}
	FModumateFQuatParam(const FQuat& v) : Value(v) {}
};

USTRUCT()
struct FModumateFloatParam
{
	GENERATED_BODY()
	
	UPROPERTY()
	float Value = 0.0f;

	FModumateFloatParam() {}
	FModumateFloatParam(float v) : Value(v) {}
};

USTRUCT()
struct FModumateIntParam
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;

	FModumateIntParam() {}
	FModumateIntParam(int32 v) : Value(v) {}
};

USTRUCT()
struct FModumateStringParam
{
	GENERATED_BODY()

	UPROPERTY()
	FString Value;

	FModumateStringParam() {}
	FModumateStringParam(const TCHAR* v) : Value(v) {}
	FModumateStringParam(const FString& v) : Value(v) {}
};

USTRUCT()
struct FModumateNameParam
{
	GENERATED_BODY()

	UPROPERTY()
	FName Value;

	FModumateNameParam() {}
	FModumateNameParam(const FName& v) : Value(v) {}
};

USTRUCT()
struct FModumateVectorArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> Values;

	FModumateVectorArrayParam(const TArray<FVector>& v) : Values(v) {}
	FModumateVectorArrayParam() {};
};

USTRUCT()
struct FModumateVector2DArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector2D> Values;

	FModumateVector2DArrayParam(const TArray<FVector2D>& v) : Values(v) {}
	FModumateVector2DArrayParam() {};
};

USTRUCT()
struct FModumateRotatorArrayParam
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FRotator> Values;

	FModumateRotatorArrayParam(const TArray<FRotator>& v) : Values(v) {}
	FModumateRotatorArrayParam() {};
};

USTRUCT()
struct FModumateQuatArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FQuat> Values;

	FModumateQuatArrayParam(const TArray<FQuat>& v) : Values(v) {}
	FModumateQuatArrayParam() {};
};

USTRUCT()
struct FModumateIntArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Values;

	FModumateIntArrayParam(const TArray<int32>& v) : Values(v) {}
	FModumateIntArrayParam() {};
};

USTRUCT()
struct FModumateFloatArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<float> Values;

	FModumateFloatArrayParam(const TArray<float>& v) : Values(v) {}
	FModumateFloatArrayParam() {};
};

USTRUCT()
struct FModumateStringArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> Values;

	FModumateStringArrayParam(const TArray<FString>& v) : Values(v) {}
	FModumateStringArrayParam() {};
};

USTRUCT()
struct FModumateNameArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> Values;

	FModumateNameArrayParam(const TArray<FName>& v) : Values(v) {}
	FModumateNameArrayParam() {};
};

USTRUCT()
struct FModumateBoolArrayParam
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<bool> Values;

	FModumateBoolArrayParam(const TArray<bool>& v) : Values(v) {}
	FModumateBoolArrayParam() {};
};

/*
Modumate Commands are text commands issued that are sent to the Unreal console
Modumate Functions are functionality that are bound to text commands
*/

class MODUMATE_API FModumateCommandParameter
{
private:
	FString StringValue;
	friend class FModumateCommand;
	friend class FModumateFunctionParameterSet;

public:
	FString ParameterName;
	FModumateCommandParameter() {}

	FModumateCommandParameter(const FString &name, const TCHAR *s)				: ParameterName(name) { FromString(s); }
	FModumateCommandParameter(const FString &name, const FString &s)			: ParameterName(name) { FromString(s); }
	FModumateCommandParameter(const FString &name, const FName &n)				: ParameterName(name) { FromName(n); }
	FModumateCommandParameter(const FString &name, const FVector &v)			: ParameterName(name) { FromVector(v); }
	FModumateCommandParameter(const FString &name, const FVector2D &v)			: ParameterName(name) { FromVector2D(v); }
	FModumateCommandParameter(const FString &name, const FRotator &v)			: ParameterName(name) { FromRotator(v); }
	FModumateCommandParameter(const FString &name, const FQuat &v)				: ParameterName(name) { FromQuat(v); }
	FModumateCommandParameter(const FString &name, int32 i)						: ParameterName(name) { FromInt(i); }
	FModumateCommandParameter(const FString &name, float v)						: ParameterName(name) { FromFloat(v); }
	FModumateCommandParameter(const FString &name, bool b)						: ParameterName(name) { FromBool(b); }
	FModumateCommandParameter(const FString &name, const TArray<FVector> &v)	: ParameterName(name) { FromVectorArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<FVector2D> &v)	: ParameterName(name) { FromVector2DArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<FRotator> &v)	: ParameterName(name) { FromRotatorArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<FQuat> &v)		: ParameterName(name) { FromQuatArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<int32> &v)		: ParameterName(name) { FromIntArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<float> &v)		: ParameterName(name) { FromFloatArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<FString> &v)	: ParameterName(name) { FromStringArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<FName> &v)		: ParameterName(name) { FromNameArray(v); }
	FModumateCommandParameter(const FString &name, const TArray<bool> &v)		: ParameterName(name) { FromBoolArray(v); }

	FModumateCommandParameter(const TCHAR *s)				: FModumateCommandParameter(FString(), s) {};
	FModumateCommandParameter(const FString &s)				: FModumateCommandParameter(FString(), s) {};
	FModumateCommandParameter(const FName &n)				: FModumateCommandParameter(FString(), n) {};
	FModumateCommandParameter(const FVector &v)				: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const FVector2D &v)			: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const FRotator &v)			: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const FQuat &v)				: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(int32 i)						: FModumateCommandParameter(FString(), i) {};
	FModumateCommandParameter(float v)						: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(bool b)						: FModumateCommandParameter(FString(), b) {};
	FModumateCommandParameter(const TArray<FVector> &v)		: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<FVector2D> &v)	: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<FRotator> &v)	: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<FQuat> &v)		: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<int32> &v)		: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<float> &v)		: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<FString> &v)		: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<FName> &v)		: FModumateCommandParameter(FString(), v) {};
	FModumateCommandParameter(const TArray<bool> &v)		: FModumateCommandParameter(FString(), v) {};

	bool Equals(const FModumateCommandParameter &rhs) const { return StringValue == rhs.StringValue; }

	FQuat AsQuat() const;
	FRotator AsRotator() const;
	FVector AsVector() const;
	FVector2D AsVector2D() const;
	float AsFloat() const;
	int32 AsInt() const;
	bool AsBool() const;
	FString AsString() const;
	FString AsJSON() const;
	FName AsName() const;

	TArray<FVector> AsVectorArray() const;
	TArray<FVector2D> AsVector2DArray() const;
	TArray<FRotator> AsRotatorArray() const;
	TArray<FQuat> AsQuatArray() const;
	TArray<int32> AsIntArray() const;
	TArray<float> AsFloatArray() const;
	TArray<FString> AsStringArray() const;
	TArray<FName> AsNameArray() const;
	TArray<bool> AsBoolArray() const;

	void FromRotator(const FRotator &r);
	void FromQuat(const FQuat &r);
	void FromVector(const FVector &v);
	void FromVector2D(const FVector2D &v);
	void FromFloat(float v);
	void FromInt(int32 i);
	void FromBool(bool b);
	void FromString(const FString &s);
	void FromJSON(const FString &json);
	void FromName(const FName &name);

	void FromVectorArray(const TArray<FVector> &v);
	void FromVector2DArray(const TArray<FVector2D> &v);
	void FromRotatorArray(const TArray<FRotator> &v);
	void FromQuatArray(const TArray<FQuat> &v);
	void FromIntArray(const TArray<int32> &v);
	void FromFloatArray(const TArray<float> &v);
	void FromStringArray(const TArray<FString> &v);
	void FromNameArray(const TArray<FName> &v);
	void FromBoolArray(const TArray<bool> &v);

	operator FRotator ()			const { return AsRotator(); }
	operator FQuat()				const { return AsQuat(); }
	operator FVector ()				const { return AsVector(); }
	operator FVector2D ()			const { return AsVector2D(); }
	operator float()				const { return AsFloat(); }
	operator int32 ()				const { return AsInt(); }
	operator bool ()				const { return AsBool(); }
	operator FString()				const { return AsString(); }
	operator FName()				const { return AsName(); }
	operator TArray<FVector>()		const { return AsVectorArray(); }
	operator TArray<FVector2D>()	const { return AsVector2DArray(); }
	operator TArray<FRotator>()		const { return AsRotatorArray(); }
	operator TArray<FQuat>()		const { return AsQuatArray(); }
	operator TArray<int32>()		const { return AsIntArray(); }
	operator TArray<float>()		const { return AsFloatArray(); }
	operator TArray<FString>()		const { return AsStringArray();  }
	operator TArray<FName>()		const { return AsNameArray(); }
	operator TArray<bool>()			const { return AsBoolArray(); }
};

class MODUMATE_API FModumateFunctionParameterSet : private TMap<FString, FString>
{
	friend class FModumateCommand;
	friend class FModumateCommandParameter;

	typedef TMap<FString, FString> FPrivateBaseClass;

public:
	FModumateCommandParameter GetValue(const FString &key, const FModumateCommandParameter &defaultValue = FModumateCommandParameter(0)) const;
	FModumateFunctionParameterSet &SetValue(const FString &key, const FModumateCommandParameter &val);

	bool HasValue(const FString &key) const;
	void GetValueNames(TArray<FString> &outNames) const;
	void ForEachProperty(TFunction<void(const FString &name, const FModumateCommandParameter &mcp)> fn) const;

	void Empty();
	void Remove(const FString &key);

	bool Matches(const FModumateFunctionParameterSet &MatchSet) const;
	int32 Num() const;

	typedef TMap<FString,FString> FStringMap;
	bool ToStringMap(FStringMap &OutMap) const;
	bool FromStringMap(const FStringMap &Map);
};

static constexpr int32 MODUMATE_COMMAND_VERSION = 1;

class MODUMATE_API FModumateCommand
{
private:

	static int32 NextCommandID;
	FModumateCommandJSON JSONValue;

	FModumateCommand & Param(const FModumateCommandParameter &param);

public:
	static const FString VersionString;
	static const FString CommandIDString;
	static const FString CommandFieldString;

	FString GetJSONString() const;

	static FModumateCommand FromJSONString(const FString &jsonString);

	int32 CachedCommandID;
	bool bCaptureForInput;

	FModumateCommand(const FString &command, bool bInCaptureForInput = false, int32 version = MODUMATE_COMMAND_VERSION);
	FModumateCommand() {}

	FModumateFunctionParameterSet GetParameterSet() const;
	void SetParameterSet(const FModumateFunctionParameterSet &params);


	template <class T>
	FModumateCommand &Param(const FString &name, const T &param)
	{
		return Param(FModumateCommandParameter(name, param));
	}
};

// TODO: refactor for TFunction
struct MODUMATE_API FModumateFunction
{
	FModumateFunction(const std::function<bool(const FModumateFunctionParameterSet &, FModumateFunctionParameterSet &)> &fn) : FN(fn) {};
	std::function<bool(const FModumateFunctionParameterSet &, FModumateFunctionParameterSet &)> FN;
};
