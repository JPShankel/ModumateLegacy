// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/UnitConversion.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "ModumateCore/ModumateTypes.h"
#include "UObject/Object.h"

#include "ModumateDimensionStatics.generated.h"

// Helper functions for managing dimensional string parsing and generation
// 
UCLASS(BlueprintType)
class MODUMATE_API UModumateDimensionStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// When rounding a decimal, how many significant digits to try to preserve.
	// (10 ^ DefaultMaxDecimalDigits) would effectively be the fixed-point multiplication factor.
	static constexpr double DefaultRoundingDigits = 2;

	// When displaying an unrounded decimal, how many digits to display in the text.
	static constexpr double DefaultDisplayDigits = 8;

	// Maximum fractional precision desired, in this case 1 / (2 ^ 6) aka 1/64ths.
	static constexpr int32 DefaultFractionMaxDenomPow = 6;

	// When rounding a value to a fraction/decimal, how far off can the value be, relative to the minimum fractional value?
	static constexpr double DefaultRoundingTolerance = 0.015625;

	static constexpr double InchesToCentimeters = 2.54;
	static constexpr double CentimetersToInches = 1.0 / InchesToCentimeters;
	static constexpr double InchesPerFoot = 12.0;

	// Efficient and culture aware number formatting and parsing, to convert a string containing a culture correct decimal representation of a number into an actual number.
	template<typename T>
	static typename TEnableIf<TIsArithmetic<T>::Value, bool>::Type
	TryParseNumber(const FString& NumberString, T& OutValue, bool bEnableGrouping = true)
	{
		return FastDecimalFormat::StringToNumber(
			*NumberString,
			FInternationalization::Get().GetCurrentCulture()->GetDecimalNumberFormattingRules(),
			bEnableGrouping ? FNumberParsingOptions::DefaultWithGrouping() : FNumberParsingOptions::DefaultNoGrouping(),
			OutValue);
	}

	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static FModumateFormattedDimension StringToFormattedDimension(const FString &dimStr);

	// Convert any decimal values into string array. [0] = Whole Number, [1] = Numerator. [2] = Denominator
	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static TArray<FString> DecimalToFraction_DEPRECATED(float inputFloat, int32 maxDenom = 64);

	// Convert decimal values into string from non-zero values in above function with format: "{0} {1}/{2}"
	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static FString DecimalToFractionString_DEPRECATED(float inches, bool bFormat = false, bool bSplitWholeNumber = false,
		EDimensionUnits UnitType = EDimensionUnits::DU_Imperial, EUnit OverrideUnit = EUnit::Unspecified, int32 maxDenom = 64);

	UFUNCTION()
	static bool RoundDecimal(double InValue, double& OutRounded, int32 NumRoundingDigits = DefaultRoundingDigits, double Tolerance = DefaultRoundingTolerance);

	UFUNCTION()
	static bool RoundFraction(double InValue, int32& OutInteger, int32& OutNumerator, int32& OutDenom, int32 MaxDenomPower = DefaultFractionMaxDenomPow, double Tolerance = DefaultRoundingTolerance);

	UFUNCTION()
	static FText InchesToDisplayText(double LengthInches, EDimensionUnits UnitType = EDimensionUnits::DU_Imperial, EUnit OverrideUnit = EUnit::Unspecified,
		int32 MaxDenomPower = DefaultFractionMaxDenomPow, double FractionalTolerance = DefaultRoundingTolerance, int32 NumRoundingDigits = DefaultRoundingDigits, int32 NumDisplayDigits = DefaultDisplayDigits);

	UFUNCTION()
	static FText CentimetersToDisplayText(double LengthCM, EDimensionUnits UnitType = EDimensionUnits::DU_Imperial, EUnit OverrideUnit = EUnit::Unspecified,
		int32 MaxDenomPower = DefaultFractionMaxDenomPow, double FractionalTolerance = DefaultRoundingTolerance, int32 NumRoundingDigits = DefaultRoundingDigits, int32 NumDisplayDigits = DefaultDisplayDigits);

	UFUNCTION()
	static float CentimetersToInches64(double Centimeters);
};
