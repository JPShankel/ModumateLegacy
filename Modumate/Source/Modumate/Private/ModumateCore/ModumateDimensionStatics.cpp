// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateDimensionStatics.h"

#include "Internationalization/Text.h"
#include "Internationalization/Culture.h"
#include "Internationalization/FastDecimalFormat.h"
#include "MathUtil.h"
#include "ModumateCore/ModumateTypes.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include <regex>



#define LOCTEXT_NAMESPACE "ModumateDimensions"

/*
* TODO: metric strings still use explicit pattern matching
* This is to be rolled in with the tokenized imperial parser below
*/
static FModumateFormattedDimension StringToFormattedDimensionMetric(const FString& dimStr)
{
	FString trimmedDimStr = dimStr.TrimStartAndEnd().Replace(TEXT(","), TEXT(""));

	FModumateFormattedDimension result;
	result.Format = EDimensionFormat::Error;
	result.FormattedString = trimmedDimStr;
	result.Centimeters = 0;

	double sign = 1.0;

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

	static std::wstring decimalString = L"[,\\.\\d*]+";
	static std::wregex decimalPattern(decimalString);

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

/*
* The Big Picture:
* The initial implentation of dimension string matching follwed the pattern given in the metric function above:
* Legal patterns for dimension strings were explicitly matched one at a time
* This pattern makes it difficult to resolve ambiguous input or badly formed but clearly intended input
* 
* For imperial units (and soon for metric as well), we switch to a tokenzing strategy, where we analyze the 
* beginning of the input stream, match it against legal input (a number or a unit designation) and then
* generate a result from the parse record.
* 
* The stages of analysis are broken down as:
* 
* Preprocesor stage: gross-level removal or alteration of known patterns from the input string
* Tokenization stage: process input string tokens and produce a record of the value and units found
* Grammar stage: analyze the unit/value record to extract feet and inches as well as report any errors 
* Compilation stage: with feet and inches isolated, produce an output in centimeters
* 
*/

FModumateFormattedDimension UModumateDimensionStatics::StringToFormattedDimension(const FString &DimStr, EDimensionUnits UnitType, EUnit DefaultUnit)
{
	/*
	* TODO: roll metric into tokenized algorithm below...meantime just check first if we are in metric
	*/
	FModumateFormattedDimension result = StringToFormattedDimensionMetric(DimStr);

	if (result.Format != EDimensionFormat::Error)
	{
		return result;
	}

	/*
	* Preprocessor stage...do any gross-level input grooming here
	*/

	// Before we get into regex, filter commas and dashes out
	FString trimmedDimStr = DimStr.TrimStartAndEnd().Replace(TEXT(","), TEXT("")).
		Replace(TEXT("ft"), TEXT("'")).
		Replace(TEXT("in"), TEXT("\"")).
		Replace(TEXT("FT"), TEXT("'")).
		Replace(TEXT("IN"), TEXT("\""));

	bool bIsNegative = false;

	// Any dash that appears before the first numerical digit indicates overall negativity
	for (int32 i = 0; i< trimmedDimStr.Len(); ++i)
	{
		if (FChar::IsDigit(trimmedDimStr[i]))
		{
			break;
		}
		if (trimmedDimStr[i] == TCHAR('-'))
		{
			bIsNegative = true;
			break;
		}
	}

	if (trimmedDimStr.IsEmpty())
	{
		return result;
	}

	// Format set to 'Error' so we can bail during tokenization
	result.Format = EDimensionFormat::Error;
	result.FormattedString = trimmedDimStr;
	result.Centimeters = 0;

	/*
	* Tokenization stage
	*/
	std::wstring dimCStr(TCHAR_TO_WCHAR(*trimmedDimStr));

	static const std::wregex fractionPattern(L"[\\d]+/[\\d]+");
	static const std::wregex floatPattern(L"([,\\d]*[.][0-9]+|[,\\d]+[.][0-9]*)");
	static const std::wregex integerPattern(L"[,\\d]+");
	static const std::wregex feetPattern(L"(ft|')+");
	static const std::wregex inchesPattern(L"(in|\")+");
	static const std::wregex garbagePattern(L"[a-zA-Z\\s-+]+");

	/*
	* Keep a record of the values we resolve during tokenization
	*/
	struct FDimension
	{
		EUnit Unit;
		double Value;
		bool IsFraction = false;
	};

	TArray<FDimension> dimensionParse;

	/*
	* Apply tokenization rules in priority order, looping until nothing clicks or we run out of input
	* The regex_search pattern used below will find a token if it is at the beginning of the input string (prefix length is zero)
	* This provides lex-style tokenization parsing which can tolerate components coming in in different orders or having garbage characters
	* 
	* The default state of the return value (result) is Error, so when we encounter an illegal parse, we imply return result as-is
	*/
	while (dimCStr.length() > 0)
	{
		std::wsmatch sm;

		//"Garbage" is any random string value that's not a unit designator or part of a number
		if (std::regex_search(dimCStr, sm, garbagePattern) && sm.prefix().length() == 0)
		{
			dimCStr = sm.suffix();
			continue;
		}

		// Find fractions before any other number type
		if (std::regex_search(dimCStr, sm, fractionPattern) && sm.prefix().length() == 0)
		{
			std::wstring frac = sm[0].str();
			auto split = frac.find(L'/');
			std::wstring numer = frac.substr(0, split);
			std::wstring denom = frac.substr(split+1, frac.length());
			double numerV, denomV;
			if (LexTryParseString(numerV, WCHAR_TO_TCHAR(numer.c_str())) && LexTryParseString(denomV, WCHAR_TO_TCHAR(denom.c_str())))
			{
				if (FMath::IsNearlyZero(denomV))
				{
					return result;
				}

				double v = numerV / denomV;
				if (dimensionParse.Num() > 0 && (dimensionParse.Last().Unit == EUnit::Unspecified || dimensionParse.Last().Unit == EUnit::Inches))
				{
					if (dimensionParse.Last().Value < 0.0)
					{
						dimensionParse.Last().Value -= v;
					}
					else
					{
						dimensionParse.Last().Value += v;
					}
				}
				else
				{
					FDimension& dim = dimensionParse.AddDefaulted_GetRef();
					dim.Unit = EUnit::Unspecified;
					dim.IsFraction = true;
					dim.Value = v;
				}
			}
			dimCStr = sm.suffix();
			continue;
		}

		// Find floats before ints because a float's int portion will misreport
		if (std::regex_search(dimCStr, sm, floatPattern) && sm.prefix().length() == 0)
		{
			double v;
			if (LexTryParseString(v,WCHAR_TO_TCHAR(sm[0].str().c_str())))
			{
				if (dimensionParse.Num() > 0 && dimensionParse.Last().Unit == EUnit::Unspecified)
				{
					dimensionParse.Last().Unit = EUnit::Feet;
				}
				FDimension& dim = dimensionParse.AddDefaulted_GetRef();
				dim.Unit = EUnit::Unspecified;
				dim.Value = v;
			}
			dimCStr = sm.suffix();
			continue;
		}

		// If we don't have a fraction or a float, see if we have a bare int
		if (std::regex_search(dimCStr, sm, integerPattern) && sm.prefix().length()==0)
		{
			double v;
			if (LexTryParseString(v,WCHAR_TO_TCHAR(sm[0].str().c_str())))
			{
				if (dimensionParse.Num() > 0 && dimensionParse.Last().Unit == EUnit::Unspecified)
				{
					dimensionParse.Last().Unit = EUnit::Feet;
				}
				FDimension& dim = dimensionParse.AddDefaulted_GetRef();
				dim.Unit = EUnit::Unspecified;
				dim.Value = v;
			}
			dimCStr = sm.suffix();
			continue;
		}

		// See if we're specifying feet
		if (std::regex_search(dimCStr, sm, feetPattern) && sm.prefix().length() == 0)
		{
			if (dimensionParse.Num() != 0)
			{
				if (dimensionParse.Last().Unit == EUnit::Inches)
				{
					return result;
				}
				dimensionParse.Last().Unit = EUnit::Feet;
			}
			dimCStr = sm.suffix();
			continue;
		}

		// See if we're specifying inches
		if (std::regex_search(dimCStr, sm, inchesPattern) && sm.prefix().length() == 0)
		{
			if (dimensionParse.Num() != 0)
			{
				if (dimensionParse.Last().Unit == EUnit::Feet)
				{
					return result;
				}
				dimensionParse.Last().Unit = EUnit::Inches;
			}
			dimCStr = sm.suffix();
			continue;
		}

		// If we didn't match any of the rules above, we're done finding input
		break;
	}

	/*
	* Grammar analysis
	*/

	//If we found no components or were unable to parse the whole string, return error	
	if (dimensionParse.Num() == 0 || dimCStr.length() > 0)
	{
		return result;
	}


	// If we got a simple bare number, use default units as guide
	if (dimensionParse.Num() == 1 && dimensionParse[0].Unit == EUnit::Unspecified && !dimensionParse[0].IsFraction)
	{
		if (UnitType == EDimensionUnits::DU_Imperial)
		{
			switch (DefaultUnit)
			{
			case EUnit::Inches:
				result.Format = EDimensionFormat::JustInches;
				result.Centimeters = (bIsNegative ? -1.0 : 1.0) * (dimensionParse[0].Value) * InchesToCentimeters;
				return result;
			default:
				result.Format = EDimensionFormat::JustFeet;
				result.Centimeters = (bIsNegative ? -1.0 : 1.0) * (InchesPerFoot * dimensionParse[0].Value) * InchesToCentimeters;
				return result;
			};
		}
		else
		{
			switch (DefaultUnit)
			{
			case EUnit::Millimeters:
				result.Format = EDimensionFormat::JustMillimeters;
				result.Centimeters = (bIsNegative ? -1.0 : 1.0) * dimensionParse[0].Value * 0.1;
				return result;
				break;
			case EUnit::Centimeters:
				result.Format = EDimensionFormat::JustCentimeters;
				result.Centimeters = (bIsNegative ? -1.0 : 1.0) * dimensionParse[0].Value;
				return result;
			default:
				result.Format = EDimensionFormat::JustMeters;
				result.Centimeters = (bIsNegative ? -1.0 : 1.0) * (100 * dimensionParse[0].Value);
				return result;
			};
		}
	}

	/*
	* Scan the parse record for feet and inches components and set final output appropriately
	* TODO: implement as a data-driven finite state machine?
	*/
	double feetV = 0, inchesV = 0;
	for (auto& dim : dimensionParse)
	{
		if (dim.Unit == EUnit::Feet)
		{
			feetV = dim.Value;
			switch (result.Format)
			{
			// Encountering the same unit twice is an error
			case EDimensionFormat::FeetAndInches:
			case EDimensionFormat::JustFeet:
			{
				result.Format = EDimensionFormat::Error;
				return result;
			}
			break;
			case EDimensionFormat::JustInches:
			{
				result.Format = EDimensionFormat::FeetAndInches;
			}
			break;

			// We'll still be in the error state if no units were specified
			case EDimensionFormat::Error:
			{
				result.Format = EDimensionFormat::JustFeet;
			}
			break;
			};
		}

		if (dim.Unit == EUnit::Inches)
		{
			inchesV = dim.Value;
			switch (result.Format)
			{
				// Encountering the same unit twice is an error
			case EDimensionFormat::FeetAndInches:
			case EDimensionFormat::JustInches:
			{
				result.Format = EDimensionFormat::Error;
				return result;
			}
			case EDimensionFormat::JustFeet:
			{
				result.Format = EDimensionFormat::FeetAndInches;
			}
			break;
			// We'll still be in the error state if no units were specified
			case EDimensionFormat::Error:
			{
				result.Format = EDimensionFormat::JustInches;
			}
			break;
			}
		}

		if (dim.Unit == EUnit::Unspecified)
		{
			switch (result.Format)
			{
			case EDimensionFormat::FeetAndInches:
			{
				result.Format = EDimensionFormat::Error;
				return result;
			}
			case EDimensionFormat::JustFeet:
			{
				inchesV = dim.Value;
				result.Format = EDimensionFormat::FeetAndInches;
			}
			break;
			case EDimensionFormat::JustInches:
			{
				feetV = dim.Value;
				result.Format = EDimensionFormat::FeetAndInches;
			}
			break;
			case EDimensionFormat::Error:
			{
				if (dim.IsFraction)
				{
					inchesV = dim.Value;
					result.Format = EDimensionFormat::JustInches;
				}
				else
				{
					feetV = dim.Value;
					result.Format = EDimensionFormat::JustFeet;
				}
			}
			break;
			}
		}
	}

	/*
	* Compilation stage: resolve final unit values
	*/

	//Fractional portions of feet are converted to inches
	float feetDecimalInches = 0;
	if (feetV < 0.0)
	{
		inchesV = -inchesV;

		feetDecimalInches = (-feetV - FMath::FloorToDouble(-feetV));
		feetV = -floor(-feetV);
	}
	else
	{
		feetDecimalInches = (feetV - FMath::FloorToDouble(feetV));
		feetV = FMath::FloorToDouble(feetV);
	}

	feetDecimalInches *= InchesPerFoot;
	if (feetV < 0.0)
	{
		inchesV -= feetDecimalInches;
	}
	else
	{
		inchesV += feetDecimalInches;
	}

	/*
	*	If a 'JustFeet' value produces inches from its fractional part, promote to FeetAndInches
	*/
	if (result.Format == EDimensionFormat::JustFeet && !FMath::IsNearlyZero(inchesV))
	{
		result.Format = EDimensionFormat::FeetAndInches;
	}

	result.Centimeters = (bIsNegative ? -1.0 : 1.0)*(InchesPerFoot * feetV + inchesV) * InchesToCentimeters;

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

	// Make the tolerance more forgiving for higher dimensionalities, since we lose precision when squaring/cubing dimensions.
	double decimalTolerance = DefaultRoundingTolerance;
	if (Dimensionality != 1)
	{
		decimalTolerance = FMathd::Pow(decimalTolerance, 1.0 / Dimensionality);
	}

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
		if ((Dimensionality == 1) && RoundFraction(lengthInchesComponent, inchesInt, inchesNumerator, inchesDenom, MaxDenomPower, FractionalTolerance))
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
		if (RoundDecimal(lengthInchesComponent, inchesComponentDecimal, NumRoundingDigits, decimalTolerance))
		{
			if (inchesComponentDecimal == inchesPerFootDim)
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
		if (RoundDecimal(lengthCM, lengthCMRounded, NumRoundingDigits, decimalTolerance))
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
