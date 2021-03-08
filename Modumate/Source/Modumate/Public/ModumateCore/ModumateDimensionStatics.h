// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
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
	static FString DecimalToFractionString_DEPRECATED(float inches, bool bFormat = false, bool bSplitWholeNumber = false, int32 maxDenom = 64);

	UFUNCTION()
	static FText InchesToImperialText(float Inches, int32 MaxDenom = 64);

	UFUNCTION()
	static FText CentimetersToImperialText(float Centimeters, int32 MaxDenom = 64);

	UFUNCTION()
	static float CentimetersToInches64(float Centimeters);

	static constexpr float InchesToCentimeters = 2.54f;
	static constexpr float CentimetersToInches = 1.0f / InchesToCentimeters;
	static constexpr float InchesPerFoot = 12.0f;
};
