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
	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion", meta = (ToolTip = "Try to parse a string that contains a number input from a user, in a localization-aware manner."))
	static bool TryParseInputNumber(const FString &NumberString, float &OutValue);

	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static FModumateFormattedDimension StringToFormattedDimension(const FString &dimStr);

	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static float StringToMetric(FString inputString, bool assumeLoneNumberAsFeet=false);

	// THIS IS ONLY USEFUL FOR StringToMetric. Look for forward slash and treat it as divide.
	UFUNCTION()
	static TArray<FString> StringToMetricCheckSlash(TArray<FString> inputStringGroup);

	// Convert any decimal values into string array. [0] = Whole Number, [1] = Numerator. [2] = Denominator
	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static TArray<FString> DecimalToFraction(float inputFloat, int32 maxDenom = 64);

	// Convert decimal values into string from non-zero values in above function with format: "{0} {1}/{2}"
	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static FString DecimalToFractionString(float inches, bool bFormat = false, bool bSplitWholeNumber = false, int32 maxDenom = 64);

	// Convert cm to imperial units (ft, in, numerator and denominator)
	UFUNCTION(BlueprintCallable, Category = "Modumate Unit Conversion")
	static FModumateImperialUnits CentimeterToImperial(const float centimeter, const int32 maxDenominator = 64);

	UFUNCTION(BlueprintCallable)
	static void CentimetersToImperialInches(float Centimeters, UPARAM(ref) TArray<int32>& Imperial);

	UFUNCTION(BlueprintCallable)
	static FText ImperialInchesToDimensionStringText(UPARAM(ref) TArray<int32>& Imperial);

	static constexpr float InchesToCentimeters = 2.54f;
	static constexpr float CentimetersToInches = 1.0f / InchesToCentimeters;
	static constexpr float InchesPerFoot = 12.0f;
};
