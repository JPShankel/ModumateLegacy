// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateDimensionStatics.h"
#include "ModumateTypes.h"
#include "ModumateUnits.h"
#include "ModumateFunctionLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include <regex>

using namespace Modumate;

FModumateFormattedDimension UModumateDimensionStatics::StringToFormattedDimension(const FString &dimStr)
{
	std::wstring dimCStr = *dimStr;

	//Matches any integer...bare integers are interpreted as inches
	static std::wstring integerString = L"\\d+";
	static std::wregex integerPattern(integerString);

	//Integer feet ie '9ft'
	static std::wstring wholeFeetString = L"(" + integerString + L")" + L"(ft|')";
	static std::wregex wholeFeetPattern(wholeFeetString);

	static std::wstring decimalString = L"\\d*(\\.\\d+)?";

	//Decimal inches ie 1in or 2.3" (can be integer)
	static std::wstring wholeInchesString = L"(" + integerString + L")" + L"(in|\")";
	static std::wregex inchesDecimal(L"(" + decimalString + L")(in|\")");

	//Feet with a whole number of inches, ie '3ft 6in'
	static std::wregex feetWholeInchesPattern(wholeFeetString + L"\\s+" + wholeInchesString);

	//A simple fraction of inches (no whole number part) ie '3/8in'
	static std::wstring inchesSimpleFractionString = L"(" + integerString + L")" + L"/" + L"(" + integerString + L")(in|\")";
	static std::wregex inchesSimpleFractionPattern(inchesSimpleFractionString);

	// Whole number of feet with simple fraction of inches ie '2ft 3/4in'
	static std::wregex feetSimpleFractionInchesPattern(wholeFeetString + L"\\s+" + inchesSimpleFractionString);

	//A complex fraction of inches with whole and frac part ie '5 3/8in'
	static std::wstring inchesComplexFractionString = L"(" + integerString + L")\\s+(" + integerString + L")" + L"/" + L"(" + integerString + L")(in|\")";
	static std::wregex inchesComplexFractionPattern(inchesComplexFractionString);

	// Whole number of feet with complex fraction of inches ie '2ft 4 3/4in'
	static std::wregex feetComplexFractionInchesPattern(wholeFeetString + L"\\s+" + inchesComplexFractionString);

	//Metric recognizes decimal places, decimal numbers are integers with optional mantissa
	static std::wregex decimalPattern(decimalString);

	//Plain meters, ie '1.2m'
	static std::wregex justMetersPattern(L"(" + decimalString + L")m");

	//Plain meters, ie '1.2cm'
	static std::wregex justCentimetersPattern(L"(" + decimalString + L")cm");

	//Meters and centimeters with meters as an integer, ie '2m 45.3cm' 
	static std::wregex metersAndCentimetersPattern(L"(" + integerString + L")m\\s+(" + decimalString + L")cm");

	FModumateFormattedDimension ret;
	ret.FormattedString = dimStr;

	std::wsmatch match;

	// Bare integers are assumed to be plain inches
	if (std::regex_match(dimCStr, match, integerPattern))
	{
		ret.Format = EDimensionFormat::JustInches;
		ret.Centimeters = FCString::Atof(*dimStr) * InchesToCentimeters;
		return ret;

	}

	// Bare decimal values are assumed to be plain centimeters
	if (std::regex_match(dimCStr, match, decimalPattern))
	{
		ret.Format = EDimensionFormat::JustCentimeters;
		ret.Centimeters = FCString::Atof(*dimStr);
		return ret;
	}

	// Simple fractions with 'in' or " ie 1/2in or 3/4"
	if (std::regex_match(dimCStr, match, inchesSimpleFractionPattern))
	{
		if (match.size() > 2)
		{
			float numer = FCString::Atof(match[1].str().c_str());
			float denom = FCString::Atof(match[2].str().c_str());
			if (!FMath::IsNearlyZero(denom))
			{
				ret.Format = EDimensionFormat::JustInches;
				ret.Centimeters = (numer / denom)* InchesToCentimeters;
			}
			else
			{
				ret.Format = EDimensionFormat::Error;

			}
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}

	// Simple fractions with 'in' or " ie 3 1/2in or 5 3/4"
	if (std::regex_match(dimCStr, match, inchesComplexFractionPattern))
	{
		if (match.size() > 3)
		{
			float whole = FCString::Atof(match[1].str().c_str());
			float numer = FCString::Atof(match[2].str().c_str());
			float denom = FCString::Atof(match[3].str().c_str());
			if (!FMath::IsNearlyZero(denom))
			{
				ret.Format = EDimensionFormat::JustInches;
				ret.Centimeters = (whole + (numer / denom))* InchesToCentimeters;
			}
			else
			{
				ret.Format = EDimensionFormat::Error;
			}
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}

	// Decimal value with 'in' or " ie 1.2in or 3.4"
	if (std::regex_match(dimCStr, match, inchesDecimal))
	{
		if (match.size() > 1)
		{
			ret.Format = EDimensionFormat::JustInches;
			ret.Centimeters = FCString::Atof(match[1].str().c_str()) * InchesToCentimeters;
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}

	// A whole number of qualified feet, ie 3ft
	if (std::regex_match(dimCStr, match, wholeFeetPattern))
	{
		if (match.size() > 1)
		{
			ret.Format = EDimensionFormat::JustFeet;
			ret.Centimeters = FCString::Atoi(match[1].str().c_str()) * 12 * InchesToCentimeters;
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}

	// A whole number of qualified feet and qualified inches, ie 3ft 5in
	if (std::regex_match(dimCStr, match, feetWholeInchesPattern))
	{
		if (match.size() > 3)
		{
			ret.Format = EDimensionFormat::FeetAndInches;
			float feet = FCString::Atof(match[1].str().c_str());
			float inches = FCString::Atof(match[3].str().c_str());
			ret.Centimeters = (feet * 12 + inches) * InchesToCentimeters;
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}

	// A whole number of qualified feet and a simple fraction of inches, ie 3ft 1/2in
	if (std::regex_match(dimCStr, match, feetSimpleFractionInchesPattern))
	{
		if (match.size() > 4)
		{
			ret.Format = EDimensionFormat::FeetAndInches;
			float numer = FCString::Atof(match[3].str().c_str());
			float denom = FCString::Atof(match[4].str().c_str());
			float feet = FCString::Atof(match[1].str().c_str());
			if (!FMath::IsNearlyZero(denom))
			{
				ret.Centimeters = (feet * 12.0f + (numer / denom)) * InchesToCentimeters;
			}
			else
			{
				ret.Format = EDimensionFormat::Error;
			}
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}

	// A whole number of feet and a complex fraction of inches, ie 6ft 3 1/4in
	if (std::regex_match(dimCStr, match, feetComplexFractionInchesPattern))
	{
		if (match.size() > 6)
		{
			ret.Format = EDimensionFormat::FeetAndInches;
			float wholeFrac = FCString::Atof(match[3].str().c_str());
			float numer = FCString::Atof(match[4].str().c_str());
			float denom = FCString::Atof(match[5].str().c_str());
			float feet = FCString::Atof(match[1].str().c_str());
			if (!FMath::IsNearlyZero(denom))
			{
				ret.Centimeters = (feet * 12 + wholeFrac + (numer / denom)) * InchesToCentimeters;
			}
			else
			{
				ret.Format = EDimensionFormat::Error;
			}
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}

	// A decimal value of meters, ie 1.234m
	if (std::regex_match(dimCStr, match, justMetersPattern))
	{
		if (match.size() > 1)
		{
			ret.Format = EDimensionFormat::JustMeters;
			ret.Centimeters = FCString::Atof(match[1].str().c_str()) * 100.0f;
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}
	// A decimal value of centimeters, ie 1.234cm
	if (std::regex_match(dimCStr, match, justCentimetersPattern))
	{
		if (match.size() > 1)
		{
			ret.Format = EDimensionFormat::JustCentimeters;
			ret.Centimeters = FCString::Atof(match[1].str().c_str());
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}
	// A number of meters and centimeters, ie 3m 16.2cm
	if (std::regex_match(dimCStr, match, metersAndCentimetersPattern))
	{
		if (match.size() > 2)
		{
			ret.Format = EDimensionFormat::MetersAndCentimeters;
			ret.Centimeters = FCString::Atof(match[1].str().c_str()) * 100.0f + FCString::Atof(match[2].str().c_str());
		}
		else
		{
			ret.Format = EDimensionFormat::Error;
		}
		return ret;
	}
	ret.Format = EDimensionFormat::Error;
	ret.Centimeters = 0;

	return ret;
}

float UModumateDimensionStatics::StringToMetric(FString inputString, bool assumeLoneNumberAsFeet)
{
	// Is the number start with negative?
	TArray<FString> inputArray = UKismetStringLibrary::GetCharacterArrayFromString(inputString);
	int32 negativeSignIndex = -1;
	int32 firstNumberIndex = -1;
	for (int32 i = 0; i < inputArray.Num(); i++)
	{
		if (inputArray[i] == TEXT("-"))
		{
			negativeSignIndex = i;
		}
		if (UKismetStringLibrary::IsNumeric(inputArray[i]))
		{
			firstNumberIndex = i;
			break;
		}
	}
	bool isNegative = (negativeSignIndex == 0) && (firstNumberIndex == 0);

	// Enter unit name here
	TArray<FString> FootNames = { TEXT("feet"), TEXT("foot"), TEXT("ft."), TEXT("ft"), TEXT("'") };
	TArray<FString> InchNames = { TEXT("inches"), TEXT("inch"), TEXT("in."), TEXT("in"), TEXT("\"") };
	TArray<FString> DecimeterNames = { TEXT("decimeter"), TEXT("deci"), TEXT("dm."), TEXT("dm"), TEXT("d") };
	TArray<FString> CentimeterNames = { TEXT("centimeter"), TEXT("centi"), TEXT("cm."), TEXT("cm"), TEXT("c") };
	TArray<FString> MeterNames = { TEXT("meter"), TEXT("m."), TEXT("m") };

	FString TotalString = inputString;
	TArray<FString> TotalStringGroup;
	FString SubStringGroup;
	float CurNumber = 0.0;
	TArray<float> SubNumbers;
	TArray<float> SubNumbersRemain;
	TArray<int32> SubNumberRmainIndex;
	int32 CurNumIndex;
	float Resultcm = 0.0;

	// Replace signs with appropriate char for recognizing string spacing
	TotalString = UKismetStringLibrary::Replace(TotalString, TEXT("+"), TEXT(" + "), ESearchCase::IgnoreCase);
	TotalString = UKismetStringLibrary::Replace(TotalString, TEXT("-"), TEXT("'"), ESearchCase::IgnoreCase);

	// Feet symbols replacement
	for (FString& FootName : FootNames)
	{
		TotalString = UKismetStringLibrary::Replace(TotalString, FootName, TEXT(" ' "), ESearchCase::IgnoreCase);
	}

	// Inches symbols replacement
	for (FString& InchName : InchNames)
	{
		TotalString = UKismetStringLibrary::Replace(TotalString, InchName, TEXT(" \" "), ESearchCase::IgnoreCase);
	}

	// Decimeter symbols replacement
	for (FString& DeciName : DecimeterNames)
	{
		TotalString = UKismetStringLibrary::Replace(TotalString, DeciName, TEXT(" d "), ESearchCase::IgnoreCase);
	}

	// Centimeter symbols replacement
	for (FString& CentiName : CentimeterNames)
	{
		TotalString = UKismetStringLibrary::Replace(TotalString, CentiName, TEXT(" c "), ESearchCase::IgnoreCase);
	}

	// Meter symbols replacement
	for (FString& MeterName : MeterNames)
	{
		TotalString = UKismetStringLibrary::Replace(TotalString, MeterName, TEXT(" m "), ESearchCase::IgnoreCase);
	}

	// Separate each letter into an array, and then group only numbers together
	TArray<FString> inputStringIndArray = UKismetStringLibrary::GetCharacterArrayFromString(TotalString);

	for (int32 i = 0; i < inputStringIndArray.Num(); i++)
	{
		bool bIsNumeric = UKismetStringLibrary::IsNumeric(inputStringIndArray[i]);

		if (bIsNumeric)
		{
			SubStringGroup = SubStringGroup + inputStringIndArray[i];
			if (i == (inputStringIndArray.Num() - 1))
			{
				TotalStringGroup.Add(SubStringGroup);
				SubStringGroup = "";
			}
		}
		else
		{
			SubStringGroup = SubStringGroup + inputStringIndArray[i];

			if ((UKismetStringLibrary::TrimTrailing(UKismetStringLibrary::Trim(SubStringGroup))).Len() > 0)
			{
				TotalStringGroup.Add(SubStringGroup);
				SubStringGroup = "";
			}
			else
			{
				SubStringGroup = "";
			}
		}

	}

	// Find slashes characters and treat them like math division
	TotalStringGroup = StringToMetricCheckSlash(TotalStringGroup);

	// If input is alone without specify feet or inches, assume is feet
	if (assumeLoneNumberAsFeet && TotalStringGroup.Num() == 1)
	{
		if (UKismetStringLibrary::IsNumeric(UKismetStringLibrary::TrimTrailing(UKismetStringLibrary::Trim(TotalStringGroup[0]))))
		{
			return FCString::Atof(*TotalStringGroup[0]) * 12 * InchesToCentimeters;
		}
	}

	// Find the number values and look at the next character to calculate for unit conversion
	for (int32 i = 0; i < TotalStringGroup.Num(); i++)
	{
		FString CurString = TotalStringGroup[i];

		if (UKismetStringLibrary::IsNumeric(UKismetStringLibrary::TrimTrailing(UKismetStringLibrary::Trim(CurString))))
		{
			if (CurNumber == 0.0)
			{
				CurNumber = FCString::Atof(*CurString);
				CurNumIndex = i;
			}
			else
			{
				SubNumbersRemain.Add(CurNumber);
				SubNumberRmainIndex.Add(CurNumIndex);
				CurNumber = FCString::Atof(*CurString);
				CurNumIndex = i;
			}
		}

		else if (UKismetStringLibrary::Contains(CurString, TEXT("'"), false, false))
		{
			SubNumbers.Add(CurNumber * 12 * InchesToCentimeters);
			CurNumber = 0.0;
			CurNumIndex = i;
		}

		else if (UKismetStringLibrary::Contains(CurString, TEXT("\""), false, false))
		{
			SubNumbers.Add(CurNumber * InchesToCentimeters);
			CurNumber = 0.0;
			CurNumIndex = i;
		}

		else if (UKismetStringLibrary::Contains(CurString, TEXT("d"), false, false))
		{
			SubNumbers.Add(CurNumber * 10.0);
			CurNumber = 0.0;
			CurNumIndex = i;
		}

		else if (UKismetStringLibrary::Contains(CurString, TEXT("c"), false, false))
		{
			SubNumbers.Add(CurNumber * 1.0);
			CurNumber = 0.0;
			CurNumIndex = i;
		}

		else if (UKismetStringLibrary::Contains(CurString, TEXT("m"), false, false))
		{
			SubNumbers.Add(CurNumber * 100.0);
			CurNumber = 0.0;
			CurNumIndex = i;
		}

		else if (CurNumber == 0.0)
		{

		}

		else
		{
			SubNumbers.Add(CurNumber * InchesToCentimeters);
			CurNumber = 0.0;
			CurNumIndex = i;
		}
	}

	// If last string is numeric, treat it as inches
	if (CurNumber != 0.0)
	{
		SubNumbers.Add(CurNumber * InchesToCentimeters);
		CurNumber = 0.0;
	}

	// Look for numeric string in context of its spacing and + sign
	for (int32 i = 0; i < SubNumbersRemain.Num(); i++)
	{
		if ((UKismetStringLibrary::Contains(TotalStringGroup[SubNumberRmainIndex[i] + 1], TEXT("+"), false, false)))
		{
			SubNumbers.Add(SubNumbersRemain[i] * InchesToCentimeters);
		}
		else
		{
			SubNumbers.Add(SubNumbersRemain[i] * 12 * InchesToCentimeters);
		}
	}

	for (float& num : SubNumbers)
	{
		Resultcm = Resultcm + num;
	}

	if (isNegative)
	{
		Resultcm = Resultcm * -1.f;
	}

	return Resultcm;
}

TArray<FString> UModumateDimensionStatics::StringToMetricCheckSlash(TArray<FString> inputStringGroup)
{
	TArray<FString> StringGroup = inputStringGroup;
	TArray<FString> FinalStringGroup;

	for (int32 i = 0; i < StringGroup.Num(); i++)
	{
		if (UKismetStringLibrary::Contains(StringGroup[i], TEXT("/"), false, false))
		{
			float SlashReplaceValue = FCString::Atof(*StringGroup[i]);

			//int32 Sizestep = FMath::Clamp(i-1, 0, 10000);
			//FString SampleString = StringGroup[FMath::Clamp(i - 1, 0, 10000)];
			FString SampleString = UKismetStringLibrary::TrimTrailing(UKismetStringLibrary::Trim(StringGroup[FMath::Clamp(i - 1, 0, 10000)]));

			float DivideValue = SlashReplaceValue / FCString::Atof(*StringGroup[i + 1]);

			if (UKismetStringLibrary::IsNumeric(SampleString))
			{
				StringGroup[i] = FString::SanitizeFloat(FCString::Atof(*SampleString) + DivideValue);
				StringGroup[i + 1] = TEXT("na");
				StringGroup[i - 1] = TEXT("na");
			}
			else
			{
				StringGroup[i] = FString::SanitizeFloat(DivideValue);
				StringGroup[i + 1] = TEXT("na");
			}
		}
	}

	for (FString& InString : StringGroup)
	{
		if (UKismetStringLibrary::Contains(InString, TEXT("na"), false, false))
		{

		}
		else
		{
			FinalStringGroup.Add(InString);
		}
	}

	return FinalStringGroup;
}

TArray<FString> UModumateDimensionStatics::DecimalToFraction(float inputFloat, int32 maxDenom)
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

FString UModumateDimensionStatics::DecimalToFractionString(float inches, bool bFormat, bool bSplitWholeNumber, int32 maxDenom)
{
	TArray<FString> imperial = UModumateDimensionStatics::DecimalToFraction(inches, maxDenom);

	FString result;
	if (imperial[0] != "0")
	{
		if (bSplitWholeNumber && inches > 12.0f)
		{
			int feet = (int)inches / 12;
			result = FString::FromInt(feet);
			if (bFormat)
			{
				result += "'";
			}
			result += " - " + FString::FromInt((int)inches - feet * 12);
		}
		else
		{
			result = imperial[0];
		}
	}
	if (imperial[1] != "0")
	{
		if (imperial[0] != "0")
		{
			result += " ";
		}
		result += imperial[1] + "/" + imperial[2];
	}

	if (bFormat)
	{
		result += "\"";
	}

	return result;
}

FModumateImperialUnits UModumateDimensionStatics::CentimeterToImperial(const float centimeter, const int32 maxDenominator)
{
	FModumateImperialUnits returnUnit;
	TArray<FString> fractions = DecimalToFraction((centimeter / InchesToCentimeters), maxDenominator);
	const float fInch = FCString::Atof(*fractions[0]);
	const int32 roundInch = round(fInch) / InchesPerFoot;
	const int32 roundModInch = round(fmod(fInch, InchesPerFoot));

	// If numerator or denominator is 0, then only return FeetWhole and InchesWhole
	if (fractions[1] == "0" || fractions[2] == "0")
	{
		if (fmod(fInch, InchesPerFoot) == 0.f)  // Can inches turn to feet without inches?
		{
			returnUnit.FeetWhole = FString::SanitizeFloat(round(fInch) / InchesPerFoot); // Feet only
		}
		else
		{
			// Use inches only if it's under or equal 12 inches
			if (roundInch == 0)
			{
				returnUnit.InchesWhole = FString::SanitizeFloat(roundModInch);
			}
			else
			{
				returnUnit.FeetWhole = FString::SanitizeFloat(roundInch);
				returnUnit.InchesWhole = FString::SanitizeFloat(roundModInch);
			}
		}
	}
	else
	{
		if (fractions[0] == "0") // If there's no whole number, use fraction only
		{
			returnUnit.InchesNumerator = fractions[1];
			returnUnit.InchesDenominator = fractions[2];
		}
		else
		{
			if (fInch > InchesPerFoot) // If more than 12 inches, use both both feet and inches
			{
				returnUnit.FeetWhole = FString::SanitizeFloat(roundInch);
				returnUnit.InchesWhole = FString::SanitizeFloat(roundModInch);
				returnUnit.InchesNumerator = fractions[1];
				returnUnit.InchesDenominator = fractions[2];
			}
			else // Only use inches
			{
				returnUnit.InchesWhole = fractions[0];
				returnUnit.InchesNumerator = fractions[1];
				returnUnit.InchesDenominator = fractions[2];
			}
		}
	}
	return returnUnit;
}

void UModumateDimensionStatics::CentimetersToImperialInches(float Centimeters, TArray<int>& ReturnImperial)
{
	ReturnImperial.Empty();
	float Inches = CentimetersToInches * Centimeters;
	int NonDecimal = (int)Inches;
	float Decimal = FMath::Abs(Inches - NonDecimal);
	ReturnImperial.Add(NonDecimal / 12);
	ReturnImperial.Add(NonDecimal % 12);
	if (Decimal >= 0.875)
	{
		ReturnImperial[0] = ReturnImperial[0] + (ReturnImperial[1] + 1) / 12;
		ReturnImperial[1] = (ReturnImperial[1] + 1) % 12;
		return;
	}
	else if (Decimal <= 0.125)
	{
		return;
	}
	else
	{
		for (int num = 1; num < 8; num++)
		{
			if ((float)num / 8 <= Decimal && Decimal <= (float)(num + 1) / 8)
			{
				//GEngine->AddOnScreenDebugMessage(0, 0.5f, FColor::Red, *FString::SanitizeFloat((float)num / denom));
				//GEngine->AddOnScreenDebugMessage(0, 0.5f, FColor::Red, *FString::SanitizeFloat((float)(num + 1) / denom));
				float ManhatDistFromLowToDec = FMath::Abs((float)num / 8 - Decimal);
				float ManhatDistFromDecToHigh = FMath::Abs((float)(num + 1) / 8 - Decimal);

				if (ManhatDistFromLowToDec < ManhatDistFromDecToHigh)
				{
					int Gcd = FMath::GreatestCommonDivisor(num, 8);
					ReturnImperial.Add(num / Gcd);
					ReturnImperial.Add(8 / Gcd);
					return;
				}
				else
				{
					int Gcd = FMath::GreatestCommonDivisor(num + 1, 8);
					ReturnImperial.Add((num + 1) / Gcd);
					ReturnImperial.Add(8 / Gcd);
					return;
				}
			}
		}
		return;
	}


}

FText UModumateDimensionStatics::ImperialInchesToDimensionStringText(TArray<int>& Imperial)
{
	FString ImperialString("");
	if (Imperial.Num() == 2)
	{
		ImperialString += FString::FromInt(Imperial[0]) + FString("' ") + FString::FromInt(Imperial[1]) + FString("'' ");
	}
	if (Imperial.Num() == 4)
	{
		ImperialString += FString::FromInt(Imperial[0]) + FString("' ") + FString::FromInt(Imperial[1]) + FString(" ");
		ImperialString += FString::FromInt(Imperial[2]) + FString("/") + FString::FromInt(Imperial[3]) + FString(" ''");
	}
	return FText::FromString(ImperialString);
}