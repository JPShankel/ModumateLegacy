// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateDimensionStatics.h"

#include "Internationalization/Text.h"
#include "Internationalization/Culture.h"
#include "Internationalization/FastDecimalFormat.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include <regex>

using namespace Modumate;

#define LOCTEXT_NAMESPACE "ModumateDimensions"

FModumateFormattedDimension UModumateDimensionStatics::StringToFormattedDimension(const FString &dimStr)
{
	double sign = 1.0;

	// Before we get into regex, determine the sign of the string (which can only appear at the beginning, otherwise it would be in subsequent regex)
	FString trimmedDimStr = dimStr.TrimStartAndEnd();

	FModumateFormattedDimension result;
	result.Format = EDimensionFormat::Error;
	result.FormattedString = trimmedDimStr;
	result.Centimeters = 0;

	if (trimmedDimStr.IsEmpty())
	{
		return result;
	}
	if (trimmedDimStr[0] == TEXT('-'))
	{
		sign = -1.0;
		trimmedDimStr.RemoveAt(0);
	}
	else if (trimmedDimStr[0] == TEXT('+'))
	{
		trimmedDimStr.RemoveAt(0);
	}

    std::wstring dimCStr(TCHAR_TO_WCHAR(*trimmedDimStr));

	//Combines multi-unit strings
	static std::wstring multiUnitSeparator = L"[\\s-]+";

	//Matches any integer...bare integers are interpreted as feet
	static std::wstring integerString = L"[,\\d]+";
	static std::wregex integerPattern(integerString);

	//Imperial unit suffixes
	static std::wstring feetSuffix = L"(ft|')";
	static std::wstring inchesSuffix = L"(in|\")";

	//Integer feet ie '9ft'
	static std::wstring wholeFeetString = L"(" + integerString + L")" + feetSuffix;
	static std::wregex wholeFeetPattern(wholeFeetString);

	static std::wstring decimalString = L"[,\\.\\d*]+";

	//Decimal inches ie 1in or 2.3" (can be integer)
	static std::wstring wholeInchesString = L"(" + integerString + L")" + L"(in|\")";
	static std::wregex inchesDecimal(L"(" + decimalString + L")" + inchesSuffix);

	//Feet with a whole number of inches, ie '3ft 6in'
	static std::wregex feetWholeInchesPattern(wholeFeetString + multiUnitSeparator + wholeInchesString);

	//A simple fraction of inches (no whole number part) ie '3/8in'
	static std::wstring inchesSimpleFractionString = L"(" + integerString + L")" + L"/" + L"(" + integerString + L")" + inchesSuffix;
	static std::wregex inchesSimpleFractionPattern(inchesSimpleFractionString);

	// Whole number of feet with simple fraction of inches ie '2ft 3/4in'
	static std::wregex feetSimpleFractionInchesPattern(wholeFeetString + multiUnitSeparator + inchesSimpleFractionString);

	//A complex fraction of inches with whole and frac part ie '5 3/8in'
	static std::wstring inchesComplexFractionString = L"(" + integerString + L")\\s+(" + integerString + L")" + L"/" + L"(" + integerString + L")" + inchesSuffix;
	static std::wregex inchesComplexFractionPattern(inchesComplexFractionString);

	// Whole number of feet with complex fraction of inches ie '2ft 4 3/4in'
	static std::wregex feetComplexFractionInchesPattern(wholeFeetString + multiUnitSeparator + inchesComplexFractionString);

	//Metric recognizes decimal places, decimal numbers are integers with optional mantissa
	static std::wregex decimalPattern(decimalString);

	//Plain feet, ie '1.2ft'
	static std::wregex justFeetPattern(L"(" + decimalString + L")" + feetSuffix);

	//Plain meters, ie '1.2m'
	static std::wregex justMetersPattern(L"(" + decimalString + L")m");

	//Plain centimeters, ie '1.2cm'
	static std::wregex justCentimetersPattern(L"(" + decimalString + L")cm");

	//Plain millimeters, ie '1.2mm'
	static std::wregex justMillimetersPattern(L"(" + decimalString + L")mm");

	//Meters and centimeters with meters as an integer, ie '2m 45.3cm' 
	static std::wregex metersAndCentimetersPattern(L"(" + integerString + L")m" + multiUnitSeparator + L"(" + decimalString + L")cm");

	std::wsmatch match;
	int32 parsedIntA = 0, parsedIntB = 0, parsedIntC = 0, parsedIntD = 0;
	double parsedDecimalA = 0.0, parsedDecimalB = 0.0;
    
    auto getMatchString = [&match](int32 MatchIndex)
    {
        return FString(WCHAR_TO_TCHAR(match[MatchIndex].str().c_str()));
    };

	// Bare integers are assumed to be plain feet
	if (std::regex_match(dimCStr, match, integerPattern))
	{
		if (UModumateDimensionStatics::TryParseNumber(trimmedDimStr, parsedIntA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedIntA * InchesToCentimeters * InchesPerFoot;
			return result;
		}
		else
		{
			return result;
		}
	}

	// Bare decimal values are assumed to be plain feet
	if (std::regex_match(dimCStr, match, decimalPattern))
	{
		if (UModumateDimensionStatics::TryParseNumber(trimmedDimStr, parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedDecimalA * InchesToCentimeters * InchesPerFoot;
			return result;
		}
		else
		{
			return result;
		}
	}

	// Simple fractions with 'in' or " ie 1/2in or 3/4"
	if (std::regex_match(dimCStr, match, inchesSimpleFractionPattern))
	{
		if ((match.size() > 2) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(2), parsedIntB))
		{
			float numer = (float)parsedIntA;
			float denom = (float)parsedIntB;
			if (!FMath::IsNearlyZero(denom))
			{
				result.Format = EDimensionFormat::JustInches;
				result.Centimeters = sign * (numer / denom) * InchesToCentimeters;
			}
		}

		return result;
	}

	// Simple fractions with 'in' or " ie 3 1/2in or 5 3/4"
	if (std::regex_match(dimCStr, match, inchesComplexFractionPattern))
	{
		if ((match.size() > 3) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(2), parsedIntB) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(3), parsedIntC))
		{
			float whole = (float)parsedIntA;
			float numer = (float)parsedIntB;
			float denom = (float)parsedIntC;
			if (!FMath::IsNearlyZero(denom))
			{
				result.Format = EDimensionFormat::JustInches;
				result.Centimeters = sign * (whole + (numer / denom)) * InchesToCentimeters;
			}
		}

		return result;
	}

	// Decimal value with 'in' or " ie 1.2in or 3.4"
	if (std::regex_match(dimCStr, match, inchesDecimal))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustInches;
			result.Centimeters = sign * parsedDecimalA * InchesToCentimeters;
		}

		return result;
	}

	// A whole number of qualified feet, ie 3ft
	if (std::regex_match(dimCStr, match, wholeFeetPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedIntA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedIntA * InchesPerFoot * InchesToCentimeters;
		}

		return result;
	}

	// A whole number of qualified feet and qualified inches, ie 3ft 5in
	if (std::regex_match(dimCStr, match, feetWholeInchesPattern))
	{
		if ((match.size() > 3) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(3), parsedIntB))
		{
			result.Format = EDimensionFormat::FeetAndInches;
			float feet = (float)parsedIntA;
			float inches = (float)parsedIntB;
			result.Centimeters = sign * ((feet * InchesPerFoot) + inches) * InchesToCentimeters;
		}

		return result;
	}

	// A whole number of qualified feet and a simple fraction of inches, ie 3ft 1/2in
	if (std::regex_match(dimCStr, match, feetSimpleFractionInchesPattern))
	{
		if ((match.size() > 4) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(3), parsedIntB) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(4), parsedIntC))
		{
			float feet = (float)parsedIntA;
			float numer = (float)parsedIntB;
			float denom = (float)parsedIntC;
			if (!FMath::IsNearlyZero(denom))
			{
				result.Format = EDimensionFormat::FeetAndInches;
				result.Centimeters = sign * ((feet * InchesPerFoot) + (numer / denom)) * InchesToCentimeters;
			}
		}

		return result;
	}

	// A whole number of feet and a complex fraction of inches, ie 6ft 3 1/4in
	if (std::regex_match(dimCStr, match, feetComplexFractionInchesPattern))
	{
		if ((match.size() > 6) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(3), parsedIntB) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(4), parsedIntC) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(5), parsedIntD))
		{
			float feet = (float)parsedIntA;
			float wholeFrac = (float)parsedIntB;
			float numer = (float)parsedIntC;
			float denom = (float)parsedIntD;
			if (!FMath::IsNearlyZero(denom))
			{
				result.Format = EDimensionFormat::FeetAndInches;
				result.Centimeters = sign * ((feet * InchesPerFoot) + wholeFrac + (numer / denom)) * InchesToCentimeters;
			}
		}

		return result;
	}

	// A decimal value of feet, ie 1.234ft
	if (std::regex_match(dimCStr, match, justFeetPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedDecimalA * InchesToCentimeters * InchesPerFoot;
		}

		return result;
	}

	// A decimal value of meters, ie 1.234m
	if (std::regex_match(dimCStr, match, justMetersPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustMeters;
			result.Centimeters = sign * parsedDecimalA * 100.0;
		}

		return result;
	}

	// A decimal value of centimeters, ie 1.234cm
	if (std::regex_match(dimCStr, match, justCentimetersPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustCentimeters;
			result.Centimeters = sign * parsedDecimalA;
		}

		return result;
	}

	// A decimal value of millimeters, ie 1.234mm
	if (std::regex_match(dimCStr, match, justMillimetersPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustMillimeters;
			result.Centimeters = sign * parsedDecimalA * 0.1;
		}

		return result;
	}

	// A number of meters and centimeters, ie 3m 16.2cm
	if (std::regex_match(dimCStr, match, metersAndCentimetersPattern))
	{
		if ((match.size() > 2) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(1), parsedDecimalA) &&
			UModumateDimensionStatics::TryParseNumber(getMatchString(2), parsedDecimalB))
		{
			result.Format = EDimensionFormat::MetersAndCentimeters;
			result.Centimeters = sign * parsedDecimalA * 100.0 + parsedDecimalB;
		}

		return result;
	}

	return result;
}

TArray<FString> UModumateDimensionStatics::DecimalToFraction_DEPRECATED(float inputFloat, int32 maxDenom)
{
	TArray<FString> FinalStrings = { TEXT("0"), TEXT("0,"), TEXT("0") };

	float WholeNumber = floor(inputFloat);
	float RemainNumber = inputFloat - WholeNumber;
	int32 Numerator = round(RemainNumber * maxDenom);
	int32 Denominator = maxDenom;
	TArray<int32> DenomList = { 64, 32, 16, 8, 4, 2, 1 };

	if (Numerator != 0)
	{
		//for (int32& CurDenom : DenomList)
		while (Denominator > 0)
		{
			if (Numerator % 2 == 0 && Denominator % 2 == 0)
			{
				Numerator /= 2;
				Denominator /= 2;
			}
			else
			{
				break;
			}
		}
	}

	if (Numerator == 1 && Denominator == 1)
	{
		Numerator = 0;
		Denominator = 0;
		WholeNumber++;
	}

	if (Numerator == 0)
	{
		Denominator = 0;
	}

	FinalStrings[0] = FString::FromInt(round(WholeNumber));
	FinalStrings[1] = FString::FromInt(Numerator);
	FinalStrings[2] = FString::FromInt(Denominator);

	return FinalStrings;
}

FString UModumateDimensionStatics::DecimalToFractionString_DEPRECATED(float inches, bool bFormat, bool bSplitWholeNumber, EDimensionUnits UnitType, EUnit OverrideUnit, int32 maxDenom)
{
	return UModumateDimensionStatics::CentimetersToDisplayText(inches * UModumateDimensionStatics::InchesToCentimeters, 1, UnitType, OverrideUnit, maxDenom).ToString();
}

bool UModumateDimensionStatics::RoundDecimal(double InValue, double& OutRounded, int32 NumRoundingDigits, double Tolerance)
{
	OutRounded = InValue;
	double sign = FMath::Sign(InValue);
	InValue *= sign;

	double roundingFactor = FMath::Pow(10, NumRoundingDigits);
	double fixedPointValue = InValue * roundingFactor;
	double fixedPointRounded = FMath::RoundHalfToZero(fixedPointValue);
	double fixedPointError = FMath::Abs(fixedPointValue - fixedPointRounded);
	if (fixedPointError >= Tolerance)
	{
		return false;
	}

	OutRounded = fixedPointRounded / roundingFactor;
	return true;
}

bool UModumateDimensionStatics::RoundFraction(double InValue, int32& OutInteger, int32& OutNumerator, int32& OutDenom, int32 MaxDenomPower, double Tolerance)
{
	OutInteger = 0;
	OutNumerator = 0;
	OutDenom = 1;

	if (InValue == 0.0)
	{
		return true;
	}

	double sign = FMath::Sign(InValue);
	InValue *= sign;
	int32 valueInteger = FMath::FloorToInt(InValue);
	double valueFraction = InValue - valueInteger;

	int32 denominator = 1 << FMath::Max(1, MaxDenomPower);
	double numeratorFull = valueFraction * denominator;

	// If the test value cannot be rounded closely enough to the maximum acceptable fractional precision, then fail.
	double numeratorRounded = FMath::RoundHalfToZero(numeratorFull);
	double numeratorError = FMath::Abs(numeratorFull - numeratorRounded);
	if (numeratorError >= Tolerance)
	{
		return false;
	}

	// Now, express the value as a fraction with the lowest power-of-2 denominator with an odd numerator.
	OutInteger = valueInteger;
	OutNumerator = static_cast<int32>(numeratorRounded);
	OutDenom = denominator;
	while ((OutNumerator % 2 == 0) && (OutNumerator != 0))
	{
		OutNumerator /= 2;
		OutDenom /= 2;
	}

	// Carry, if the result doesn't need to be a fraction
	if (OutDenom == 1)
	{
		OutInteger++;
		OutNumerator = 0;
	}

	OutInteger *= sign;

	return true;
}

FText UModumateDimensionStatics::InchesToDisplayText(double LengthInches, int32 Dimensionality, EDimensionUnits UnitType, EUnit OverrideUnit,
	int32 MaxDenomPower, double FractionalTolerance, int32 NumRoundingDigits, int32 NumDisplayDigits)
{
	bool bNegative = LengthInches < 0;
	if (bNegative)
	{
		LengthInches *= -1.0;
	}

	FText dimPrefix;
	FText feetIndicator,inchesIndicator;
	switch (Dimensionality)
	{
	case 2: dimPrefix = LOCTEXT("Dimensionality2", "sq."); feetIndicator = LOCTEXT("DimensionalityFt", "ft."); inchesIndicator = LOCTEXT("DimensionalityIn", "in."); break;
	case 3: dimPrefix = LOCTEXT("Dimensionality3", "cu."); feetIndicator = LOCTEXT("DimensionalityFt", "ft."); inchesIndicator = LOCTEXT("DimensionalityIn", "in."); break;
	default: feetIndicator = LOCTEXT("DimensionalitySingleQuote", "'"); inchesIndicator = LOCTEXT("DimensionalityDoubleQuote", "\"");
	};

	// Set up the fallback decimal formatting for both imperial and metric formats.
	FNumberFormattingOptions decimalFormat;
	decimalFormat.MaximumFractionalDigits = NumDisplayDigits;
	double decimalLength = 0.0;
	FText decimalSuffix = FText::GetEmpty();

	switch (UnitType)
	{
	case EDimensionUnits::DU_Imperial:
	{
		double inchesPerFootDim = FMath::Pow(InchesPerFoot, Dimensionality);
		double lengthFeet = LengthInches / inchesPerFootDim;
		int32 lengthFeetComponent = FMath::TruncToInt(lengthFeet);
		double lengthInchesComponent = LengthInches - (lengthFeetComponent * inchesPerFootDim);

		// First, try to express the inches component of the length as a mixed number, potentially starting with the feet beforehand.
		int32 inchesInt, inchesNumerator, inchesDenom;
		if (RoundFraction(lengthInchesComponent, inchesInt, inchesNumerator, inchesDenom, MaxDenomPower, FractionalTolerance))
		{
			if (inchesInt == inchesPerFootDim)
			{
				lengthFeetComponent++;
				inchesInt = 0;
			}

			FText signText = bNegative ? FText::FromString(TEXT("-")) : FText::GetEmpty();
			FText feetText = (lengthFeetComponent > 0) ? FText::Format(LOCTEXT("feet", "{0}{1}{2}"), lengthFeetComponent, dimPrefix,feetIndicator) : FText::GetEmpty();
			FText inchesText;

			if (inchesNumerator != 0)
			{
				if (inchesInt != 0)
				{
					inchesText = FText::Format(LOCTEXT("inches_with_frac", "{0} {1}/{2}{3}{4}"), inchesInt, inchesNumerator, inchesDenom, dimPrefix,inchesIndicator);
				}
				else
				{
					inchesText = FText::Format(LOCTEXT("inches_only_frac", "{0}/{1}{2}{3}"), inchesNumerator, inchesDenom, dimPrefix,inchesIndicator);
				}
			}
			else
			{
				inchesText = FText::Format(LOCTEXT("inches", "{0}{1}{2}"), inchesInt, dimPrefix,inchesIndicator);
			}

			// if there are both feet and inches, separate with a hyphen, otherwise use the one that exists
			FText feetInchJoinText = (!feetText.IsEmpty() && !inchesText.IsEmpty()) ? FText::FromString(TEXT("-")) : FText::GetEmpty();

			return FText::Format(LOCTEXT("feet_and_inches", "{0}{1}{2}{3}"), signText, feetText, feetInchJoinText, inchesText);
		}

		// Otherwise, try to express the imperial length as a rounded decimal,
		// if it's close enough to the given number of digits that it was likely entered that way (e.g. 0.3" -> 0.29999999999999999"),
		// but not if it's off by more than the desired number of digits (e.g. sqrt(2)" -> 1.41421356237309505")
		FNumberFormattingOptions numberFormat;
		double inchesComponentDecimal;
		if (RoundDecimal(lengthInchesComponent, inchesComponentDecimal, NumRoundingDigits))
		{
			if (inchesComponentDecimal == InchesPerFoot)
			{
				lengthFeetComponent++;
				inchesComponentDecimal = 0.0;
			}

			decimalFormat.MaximumFractionalDigits = NumRoundingDigits;
		}

		if (lengthFeetComponent > 0)
		{
			decimalLength = lengthFeet;
			decimalSuffix = FText::Format(LOCTEXT("decimal_feet_suffix", "{0}{1}"), dimPrefix,feetIndicator);
			decimalFormat.MaximumFractionalDigits++;
		}
		else
		{
			decimalLength = LengthInches;
			decimalSuffix = FText::Format(LOCTEXT("decimal_inches_suffix", "{0}{1}"), dimPrefix,inchesIndicator);
		}
	}
	break;
	case EDimensionUnits::DU_Metric:
	{
		double lengthCM = LengthInches * FMath::Pow(UModumateDimensionStatics::InchesToCentimeters,Dimensionality);
		double lengthCMRounded;
		if (RoundDecimal(lengthCM, lengthCMRounded, NumRoundingDigits))
		{
			decimalFormat.MaximumFractionalDigits = NumRoundingDigits;
		}

		EUnit displayUnit = OverrideUnit;
		if (displayUnit == EUnit::Unspecified)
		{
			if (lengthCMRounded < 1.0)
			{
				displayUnit = EUnit::Millimeters;
			}
			else if (lengthCMRounded < 100.0)
			{
				displayUnit = EUnit::Centimeters;
			}
			else
			{
				displayUnit = EUnit::Meters;
			}
		}

		double conversionFromCM = 1.0;
		FText metricUnitSuffix = FText::GetEmpty();
		switch (displayUnit)
		{
		case EUnit::Millimeters:
			conversionFromCM = FMath::Pow(10.0,Dimensionality);
			decimalSuffix = Dimensionality > 1 ? FText::Format(LOCTEXT("MetricSuffixMM", "mm{0}"),Dimensionality) : LOCTEXT("MetricSuffixMM", "mm");
			decimalFormat.MaximumFractionalDigits--;
			break;
		case EUnit::Centimeters:
			decimalSuffix = LOCTEXT("MetricSuffixCM", "cm");
			break;
		case EUnit::Meters:
			conversionFromCM = FMath::Pow(0.01,Dimensionality);
			decimalSuffix = Dimensionality > 1 ? FText::Format(LOCTEXT("MetricSuffixM", "m{0}"), Dimensionality) : LOCTEXT("MetricSuffixM", "m");
			decimalFormat.MaximumFractionalDigits += 2;
			break;
		default:
			return FText::GetEmpty();
		}

		decimalLength = lengthCMRounded * conversionFromCM;
	}
	break;
	default:
		return FText::GetEmpty();
	}

	return FText::Format(LOCTEXT("decimal_format", "{0}{1}"), FText::AsNumber(decimalLength, &decimalFormat), decimalSuffix);
}

FText UModumateDimensionStatics::CentimetersToDisplayText(double LengthCM, int32 Dimensionality,
	EDimensionUnits UnitType, EUnit OverrideUnit, int32 MaxDenomPower, double FractionalTolerance, int32 NumRoundingDigits, int32 NumDisplayDigits)
{
	return UModumateDimensionStatics::InchesToDisplayText(LengthCM * FMath::Pow(UModumateDimensionStatics::CentimetersToInches,Dimensionality), Dimensionality,
		UnitType, OverrideUnit, MaxDenomPower, FractionalTolerance, NumRoundingDigits, NumDisplayDigits);
}

float UModumateDimensionStatics::CentimetersToInches64(double Centimeters)
{
	return FMath::RoundHalfFromZero(64.0 * Centimeters * UModumateDimensionStatics::CentimetersToInches) / 64.0;
}

#undef LOCTEXT_NAMESPACE
