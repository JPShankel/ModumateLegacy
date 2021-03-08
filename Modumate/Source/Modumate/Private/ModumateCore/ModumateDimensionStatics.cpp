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
	float sign = 1.0f;

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
		sign = -1.0f;
		trimmedDimStr.RemoveAt(0);
	}
	else if (trimmedDimStr[0] == TEXT('+'))
	{
		trimmedDimStr.RemoveAt(0);
	}

	std::wstring dimCStr = *trimmedDimStr;

	//Combines multi-unit strings
	static std::wstring multiUnitSeparator = L"[\\s-]+";

	//Matches any integer...bare integers are interpreted as feet
	static std::wstring integerString = L"[,\\d]+";
	static std::wregex integerPattern(integerString);

	//Integer feet ie '9ft'
	static std::wstring wholeFeetString = L"(" + integerString + L")" + L"(ft|')";
	static std::wregex wholeFeetPattern(wholeFeetString);

	static std::wstring decimalString = L"[,\\.\\d*]+";

	//Decimal inches ie 1in or 2.3" (can be integer)
	static std::wstring wholeInchesString = L"(" + integerString + L")" + L"(in|\")";
	static std::wregex inchesDecimal(L"(" + decimalString + L")(in|\")");

	//Feet with a whole number of inches, ie '3ft 6in'
	static std::wregex feetWholeInchesPattern(wholeFeetString + multiUnitSeparator + wholeInchesString);

	//A simple fraction of inches (no whole number part) ie '3/8in'
	static std::wstring inchesSimpleFractionString = L"(" + integerString + L")" + L"/" + L"(" + integerString + L")(in|\")";
	static std::wregex inchesSimpleFractionPattern(inchesSimpleFractionString);

	// Whole number of feet with simple fraction of inches ie '2ft 3/4in'
	static std::wregex feetSimpleFractionInchesPattern(wholeFeetString + multiUnitSeparator + inchesSimpleFractionString);

	//A complex fraction of inches with whole and frac part ie '5 3/8in'
	static std::wstring inchesComplexFractionString = L"(" + integerString + L")\\s+(" + integerString + L")" + L"/" + L"(" + integerString + L")(in|\")";
	static std::wregex inchesComplexFractionPattern(inchesComplexFractionString);

	// Whole number of feet with complex fraction of inches ie '2ft 4 3/4in'
	static std::wregex feetComplexFractionInchesPattern(wholeFeetString + multiUnitSeparator + inchesComplexFractionString);

	//Metric recognizes decimal places, decimal numbers are integers with optional mantissa
	static std::wregex decimalPattern(decimalString);

	//Plain feet, ie '1.2ft'
	static std::wregex justFeetPattern(L"(" + decimalString + L")ft");

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

	// Bare integers are assumed to be plain feet
	if (std::regex_match(dimCStr, match, integerPattern))
	{
		if (UModumateDimensionStatics::TryParseNumber(dimCStr.c_str(), parsedIntA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedIntA * InchesToCentimeters * 12;
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
		if (UModumateDimensionStatics::TryParseNumber(dimCStr.c_str(), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedDecimalA * InchesToCentimeters * 12;
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
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(match[2].str().c_str(), parsedIntB))
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
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(match[2].str().c_str(), parsedIntB) &&
			UModumateDimensionStatics::TryParseNumber(match[3].str().c_str(), parsedIntC))
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
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedDecimalA))
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
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedIntA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedIntA * 12 * InchesToCentimeters;
		}

		return result;
	}

	// A whole number of qualified feet and qualified inches, ie 3ft 5in
	if (std::regex_match(dimCStr, match, feetWholeInchesPattern))
	{
		if ((match.size() > 3) &&
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(match[3].str().c_str(), parsedIntB))
		{
			result.Format = EDimensionFormat::FeetAndInches;
			float feet = (float)parsedIntA;
			float inches = (float)parsedIntB;
			result.Centimeters = sign * (feet * 12 + inches) * InchesToCentimeters;
		}

		return result;
	}

	// A whole number of qualified feet and a simple fraction of inches, ie 3ft 1/2in
	if (std::regex_match(dimCStr, match, feetSimpleFractionInchesPattern))
	{
		if ((match.size() > 4) &&
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(match[3].str().c_str(), parsedIntB) &&
			UModumateDimensionStatics::TryParseNumber(match[4].str().c_str(), parsedIntC))
		{
			float feet = (float)parsedIntA;
			float numer = (float)parsedIntB;
			float denom = (float)parsedIntC;
			if (!FMath::IsNearlyZero(denom))
			{
				result.Format = EDimensionFormat::FeetAndInches;
				result.Centimeters = sign * (feet * 12.0f + (numer / denom)) * InchesToCentimeters;
			}
		}

		return result;
	}

	// A whole number of feet and a complex fraction of inches, ie 6ft 3 1/4in
	if (std::regex_match(dimCStr, match, feetComplexFractionInchesPattern))
	{
		if ((match.size() > 6) &&
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedIntA) &&
			UModumateDimensionStatics::TryParseNumber(match[3].str().c_str(), parsedIntB) &&
			UModumateDimensionStatics::TryParseNumber(match[4].str().c_str(), parsedIntC) &&
			UModumateDimensionStatics::TryParseNumber(match[5].str().c_str(), parsedIntD))
		{
			float feet = (float)parsedIntA;
			float wholeFrac = (float)parsedIntB;
			float numer = (float)parsedIntC;
			float denom = (float)parsedIntD;
			if (!FMath::IsNearlyZero(denom))
			{
				result.Format = EDimensionFormat::FeetAndInches;
				result.Centimeters = sign * (feet * 12 + wholeFrac + (numer / denom)) * InchesToCentimeters;
			}
		}

		return result;
	}

	// A decimal value of feet, ie 1.234ft
	if (std::regex_match(dimCStr, match, justFeetPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustFeet;
			result.Centimeters = sign * parsedDecimalA * InchesToCentimeters * 12;
		}

		return result;
	}

	// A decimal value of meters, ie 1.234m
	if (std::regex_match(dimCStr, match, justMetersPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustMeters;
			result.Centimeters = sign * parsedDecimalA * 100.0f;
		}

		return result;
	}

	// A decimal value of centimeters, ie 1.234cm
	if (std::regex_match(dimCStr, match, justCentimetersPattern))
	{
		if ((match.size() > 1) &&
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedDecimalA))
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
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedDecimalA))
		{
			result.Format = EDimensionFormat::JustMillimeters;
			result.Centimeters = sign * parsedDecimalA * 0.1f;
		}

		return result;
	}

	// A number of meters and centimeters, ie 3m 16.2cm
	if (std::regex_match(dimCStr, match, metersAndCentimetersPattern))
	{
		if ((match.size() > 2) &&
			UModumateDimensionStatics::TryParseNumber(match[1].str().c_str(), parsedDecimalA) &&
			UModumateDimensionStatics::TryParseNumber(match[2].str().c_str(), parsedDecimalB))
		{
			result.Format = EDimensionFormat::MetersAndCentimeters;
			result.Centimeters = sign * parsedDecimalA * 100.0f + parsedDecimalB;
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

FString UModumateDimensionStatics::DecimalToFractionString_DEPRECATED(float inches, bool bFormat, bool bSplitWholeNumber, int32 maxDenom)
{
	return UModumateDimensionStatics::CentimetersToImperialText(inches * UModumateDimensionStatics::InchesToCentimeters, maxDenom).ToString();
}

FText UModumateDimensionStatics::InchesToImperialText(float Length, int32 MaxDenom)
{
	bool bNegative = Length < 0;
	if (bNegative)
	{
		Length *= -1.0f;
	}

	int32 feet = Length / 12;

	Length -= (feet * 12);

	int32 inches = Length;

	Length -= inches;

	Length *= MaxDenom;

	// rounding here allows for rounding based on the tolerance
	// (ex. rounding to the nearest 1/64")
	int32 numerator = FMath::RoundHalfToZero(Length);
	int32 denominator = MaxDenom;
	while (numerator % 2 == 0 && numerator != 0)
	{
		numerator /= 2;
		denominator /= 2;
	}
	// carry
	if (denominator == 1)
	{
		inches++;
		numerator = 0;
	}
	if (inches == 12)
	{
		feet++;
		inches -= 12;
	}

	FText signText = bNegative ? FText::FromString(TEXT("-")) : FText::GetEmpty();
	FText feetText = feet != 0 ? FText::Format(LOCTEXT("feet", "{0}'"), feet) : FText::GetEmpty();
	FText inchesText;

	if (numerator != 0)
	{
		if (inches != 0)
		{
			inchesText = FText::Format(LOCTEXT("inches_with_frac", "{0} {1}/{2}\""), inches, numerator, denominator);
		}
		else
		{
			inchesText = FText::Format(LOCTEXT("inches_only_frac", "{0}/{1}\""), numerator, denominator);
		}
	}
	else
	{
		inchesText = FText::Format(LOCTEXT("inches", "{0}\""), inches);
	}

	// if there are both feet and inches, separate with a hyphen, otherwise use the one that exists
	FText feetInchJoinText = (!feetText.IsEmpty() && !inchesText.IsEmpty()) ? FText::FromString(TEXT("-")) : FText::GetEmpty();

	return FText::Format(LOCTEXT("feet_and_inches", "{0}{1}{2}{3}"), signText, feetText, feetInchJoinText, inchesText);
}

FText UModumateDimensionStatics::CentimetersToImperialText(float Length, int32 MaxDenom)
{
	return UModumateDimensionStatics::InchesToImperialText(Length * UModumateDimensionStatics::CentimetersToInches, MaxDenom);
}

float UModumateDimensionStatics::CentimetersToInches64(float Centimeters)
{
	return FMath::RoundHalfFromZero(64.0f * Centimeters * UModumateDimensionStatics::CentimetersToInches) / 64.0f;
}

#undef LOCTEXT_NAMESPACE
