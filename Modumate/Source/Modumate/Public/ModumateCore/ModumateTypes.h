// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine.h"
#include "Runtime/Engine/Classes/Engine/DataTable.h"
#include "Runtime/Engine/Classes/Materials/Material.h"
#include "ModumateTypes.generated.h"


#define MOD_ID_NONE 0


USTRUCT(BlueprintType)
struct FLineIndex
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate LineIndex")
	int32 A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate LineIndex")
	int32 B;
};

USTRUCT(BlueprintType)
struct FTriangle2D
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate LineIndex")
	int32 A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate LineIndex")
	int32 B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate LineIndex")
	int32 C;
};

USTRUCT(BlueprintType)
struct FWallEdges
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Vector")
	TArray<FVector> PrimaryEdgeCPs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Vector")
	TArray<FVector> SecondaryEdgeCPs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Vector")
	TArray<FVector> PrimaryEdgeCPsReversed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Vector")
	TArray<FVector> SecondaryEdgeCPsReversed;
};

USTRUCT()
struct FModumateCommandJSON
{
	GENERATED_BODY();
	UPROPERTY()
	TMap<FString, FString> Parameters;
};

USTRUCT()
struct FModumateFVectorParam
{
	GENERATED_BODY();
	UPROPERTY()
	FVector Value = FVector::ZeroVector;

	FModumateFVectorParam() {};
	FModumateFVectorParam(const FVector &v) : Value(v) {}
};

USTRUCT()
struct FModumateFVector2DParam
{
	GENERATED_BODY();
	UPROPERTY()
	FVector2D Value = FVector2D::ZeroVector;

	FModumateFVector2DParam() {}
	FModumateFVector2DParam(const FVector2D &v) : Value(v) {}
};

USTRUCT()
struct FModumateFRotatorParam
{
	GENERATED_BODY();
	UPROPERTY()
	FRotator Value = FRotator::ZeroRotator;

	FModumateFRotatorParam() {}
	FModumateFRotatorParam(const FRotator &v) : Value(v) {}
};

USTRUCT()
struct FModumateFQuatParam
{
	GENERATED_BODY();
	UPROPERTY()
	FQuat Value = FQuat::Identity;

	FModumateFQuatParam() {}
	FModumateFQuatParam(const FQuat &v) : Value(v) {}
};

USTRUCT()
struct FModumateFloatParam
{
	GENERATED_BODY();
	UPROPERTY()
	float Value = 0.0f;

	FModumateFloatParam() {}
	FModumateFloatParam(float v) : Value(v) {}
};

USTRUCT()
struct FModumateIntParam
{
	GENERATED_BODY();
	UPROPERTY()
	int32 Value = 0;

	FModumateIntParam() {}
	FModumateIntParam(int32 v) : Value(v) {}
};

USTRUCT()
struct FModumateStringParam
{
	GENERATED_BODY();
	UPROPERTY()
	FString Value;

	FModumateStringParam() {}
	FModumateStringParam(const TCHAR *v) : Value(v) {}
	FModumateStringParam(const FString &v) : Value(v) {}
};

USTRUCT()
struct FModumateNameParam
{
	GENERATED_BODY();
	UPROPERTY()
	FName Value;

	FModumateNameParam() {}
	FModumateNameParam(const FName &v) : Value(v) {}
};

USTRUCT()
struct FModumateVectorArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<FVector> Values;

	FModumateVectorArrayParam(const TArray<FVector> &v) : Values(v) {}
	FModumateVectorArrayParam() {};
};

USTRUCT()
struct FModumateVector2DArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<FVector2D> Values;

	FModumateVector2DArrayParam(const TArray<FVector2D> &v) : Values(v) {}
	FModumateVector2DArrayParam() {};
};

USTRUCT()
struct FModumateRotatorArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<FRotator> Values;

	FModumateRotatorArrayParam(const TArray<FRotator> &v) : Values(v) {}
	FModumateRotatorArrayParam() {};
};

USTRUCT()
struct FModumateQuatArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<FQuat> Values;

	FModumateQuatArrayParam(const TArray<FQuat> &v) : Values(v) {}
	FModumateQuatArrayParam() {};
};

USTRUCT()
struct FModumateIntArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<int32> Values;

	FModumateIntArrayParam(const TArray<int32> &v) : Values(v) {}
	FModumateIntArrayParam() {};
};

USTRUCT()
struct FModumateFloatArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<float> Values;

	FModumateFloatArrayParam(const TArray<float> &v) : Values(v) {}
	FModumateFloatArrayParam() {};
};

USTRUCT()
struct FModumateStringArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<FString> Values;

	FModumateStringArrayParam(const TArray<FString> &v) : Values(v) {}
	FModumateStringArrayParam() {};
};

USTRUCT()
struct FModumateNameArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<FName> Values;

	FModumateNameArrayParam(const TArray<FName> &v) : Values(v) {}
	FModumateNameArrayParam() {};
};

USTRUCT()
struct FModumateBoolArrayParam
{
	GENERATED_BODY();
	UPROPERTY()
	TArray<bool> Values;

	FModumateBoolArrayParam(const TArray<bool> &v) : Values(v) {}
	FModumateBoolArrayParam() {};
};

USTRUCT(BlueprintType)
struct FModumateImperialUnits
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Unit")
	FString FeetWhole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Unit")
	FString InchesWhole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Unit")
	FString InchesNumerator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modumate Unit")
	FString InchesDenominator;
};

USTRUCT()
struct MODUMATE_API FVector2DPair
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Key;

	UPROPERTY()
	FVector2D Value;

	FVector2DPair() {}
	FVector2DPair(FVector2D InKey, FVector2D InValue) : Key(InKey), Value(InValue) {}
};

USTRUCT()
struct MODUMATE_API FModumateVectorPair
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Key;

	UPROPERTY()
	FVector Value;

	FModumateVectorPair() {}
	FModumateVectorPair(FVector InKey, FVector InValue) : Key(InKey), Value(InValue) {}
};

USTRUCT()
struct MODUMATE_API FModumateIntMap
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<int32, int32> Map;
};


UENUM(BlueprintType)
enum class EDimensionFormat : uint8
{
	None,
	FeetAndInches,
	JustFeet,
	JustInches,
	MetersAndCentimeters,
	JustMeters,
	JustCentimeters,
	JustMillimeters, 
	Error
};

USTRUCT(BlueprintType)
struct MODUMATE_API FModumateFormattedDimension
{
	GENERATED_BODY();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modumate Unit")
	EDimensionFormat Format = EDimensionFormat::None;

	FString FormattedString;
	float Centimeters = 0.0f;
};

UENUM(BlueprintType)
enum class EDoorOperationType : uint8
{
	None,
	Barn,
	Bifold,
	Pocket,
	Sliding,
	Swing
};
