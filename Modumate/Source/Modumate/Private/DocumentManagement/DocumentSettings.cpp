// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/DocumentSettings.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/ModumateSettingsMenu.h"


// 1/64", 1/32", 1/16", 1/8", 1/4", 1/2", 1", 2", 4", 6", 1'
const TArray<int32> FDocumentSettings::ImperialDistIncrementMultipliers({ 1, 2, 4, 8, 16, 32, 64, 128, 256, 384, 768 });

// 0.5mm, 1mm, 2mm, 5mm, 1cm, 2cm, 5cm, 10cm, 20cm, 50cm, 1m
const TArray<int32> FDocumentSettings::MetricDistIncrementMultipliers({ 1, 2, 4, 10, 20, 40, 100, 200, 400, 1000, 2000 });


bool FDocumentSettings::ToWebProjectSettings(FWebProjectSettings& OutSettings) const
{
	// Build options for units
	auto dimensionPrefEnum = StaticEnum<EDimensionPreference>();
	int32 numEnums = dimensionPrefEnum->NumEnums();
	if (dimensionPrefEnum->ContainsExistingMax())
	{
		numEnums--;
	}
	for (int32 dimensionPrefIdx = 0; dimensionPrefIdx < numEnums; ++dimensionPrefIdx)
	{
		EDimensionPreference dimensionPrefValue = static_cast<EDimensionPreference>(dimensionPrefEnum->GetValueByIndex(dimensionPrefIdx));
		FText dimensionPrefDisplayName = dimensionPrefEnum->GetDisplayNameTextByIndex(dimensionPrefIdx);
		FString dimensionPrefDisplayString = dimensionPrefDisplayName.ToString();
		OutSettings.units.options.Add(dimensionPrefDisplayString, dimensionPrefDisplayString);
	}

	// Build value for units
	EDimensionPreference curDimensionPref = EDimensionPreference::FeetAndInches;
	if (UModumateSettingsMenu::GetDimensionPrefFromTypes(DimensionType, DimensionUnit, curDimensionPref))
	{
		int32 curDimensionPrefIndex = dimensionPrefEnum->GetIndexByValue((int64)curDimensionPref);
		OutSettings.units.value = dimensionPrefEnum->GetDisplayNameTextByIndex(curDimensionPrefIndex).ToString();
	}

	// Get current increments
	double minDistIncrement = 0.0;
	double defaultDistIncrement = 0.0;
	TArray<int32> distIncrementMultipliers;
	switch (DimensionType)
	{
	case EDimensionUnits::DU_Imperial:
		minDistIncrement = MinImperialDistIncrementCM;
		distIncrementMultipliers = ImperialDistIncrementMultipliers;
		defaultDistIncrement = distIncrementMultipliers[DefaultImperialMultiplierIdx] * minDistIncrement;
		break;
	case EDimensionUnits::DU_Metric:
		minDistIncrement = MinMetricDistIncrementCM;
		distIncrementMultipliers = MetricDistIncrementMultipliers;
		defaultDistIncrement = distIncrementMultipliers[DefaultMetricMultiplierIdx] * minDistIncrement;
		break;
	}

	// Build value and options for increments
	OutSettings.increment.value = FString::SanitizeFloat(defaultDistIncrement);

	for (int32 multiplierIdx = 0; multiplierIdx < distIncrementMultipliers.Num(); ++multiplierIdx)
	{
		int32 distIncrementMultiplier = distIncrementMultipliers[multiplierIdx];
		double distIncrement = minDistIncrement * distIncrementMultiplier;
		FText distIncrementText = UModumateDimensionStatics::CentimetersToDisplayText(distIncrement, 1, DimensionType, DimensionUnit);
		FString distIncrementString = distIncrementText.ToString();
		OutSettings.increment.options.Add(distIncrementString, distIncrementString);

		if (distIncrement == MinimumDistanceIncrement)
		{
			OutSettings.increment.value = distIncrementString;
		}
	}

	OutSettings.latitude.value = FString::SanitizeFloat(Latitude);
	OutSettings.longitude.value = FString::SanitizeFloat(Longitude);
	OutSettings.trueNorth.value = FString::SanitizeFloat(TrueNorthDegree);

	return true;
}

bool FDocumentSettings::FromWebProjectSettings(const FWebProjectSettings& InSettings)
{
	// TODO: Majority of this code is to find the current dimension similar ToWebProjectSettings()
	// Will consolidate most logic here and from UI/ModumateSettingsMenu.h once setting menu UI is out of UUserWidget
	
	// Find unit
	bool bUnitFound = false;
	EDimensionPreference curDimensionPref = EDimensionPreference::FeetAndInches;
	auto dimensionPrefEnum = StaticEnum<EDimensionPreference>();
	for (int32 dimensionPrefIdx = 0; dimensionPrefIdx < dimensionPrefEnum->NumEnums(); ++dimensionPrefIdx)
	{
		FString dimensionPrefName = dimensionPrefEnum->GetDisplayNameTextByIndex(dimensionPrefIdx).ToString();
		if (dimensionPrefName == InSettings.units.value)
		{
			curDimensionPref = static_cast<EDimensionPreference>(dimensionPrefEnum->GetValueByIndex(dimensionPrefIdx));
			bUnitFound = true;
			break;
		}
	}
	UModumateSettingsMenu::GetDimensionTypesFromPref(curDimensionPref, DimensionType, DimensionUnit);

	// Find increment
	bool bIncrementFound = false;
	double minDistIncrement = 0.0;
	TArray<int32> distIncrementMultipliers;
	switch (DimensionType)
	{
	case EDimensionUnits::DU_Imperial:
		minDistIncrement = MinImperialDistIncrementCM;
		distIncrementMultipliers = ImperialDistIncrementMultipliers;
		break;
	case EDimensionUnits::DU_Metric:
		minDistIncrement = MinMetricDistIncrementCM;
		distIncrementMultipliers = MetricDistIncrementMultipliers;
		break;
	}
	for (int32 multiplierIdx = 0; multiplierIdx < distIncrementMultipliers.Num(); ++multiplierIdx)
	{
		int32 distIncrementMultiplier = distIncrementMultipliers[multiplierIdx];
		double distIncrement = minDistIncrement * distIncrementMultiplier;
		FString distIncrementString = UModumateDimensionStatics::CentimetersToDisplayText(distIncrement, 1, DimensionType, DimensionUnit).ToString();
		if (distIncrementString == InSettings.increment.value)
		{
			MinimumDistanceIncrement = distIncrement;
			bIncrementFound = true;
			break;
		}
	}

	Latitude = FCString::Atof(*InSettings.latitude.value);
	Longitude = FCString::Atof(*InSettings.longitude.value);
	TrueNorthDegree = FCString::Atof(*InSettings.trueNorth.value);

	return bUnitFound && bIncrementFound;
}

FDocumentSettings::FDocumentSettings()
{
	MinimumDistanceIncrement = ImperialDistIncrementMultipliers[DefaultImperialMultiplierIdx] * MinImperialDistIncrementCM;
}

bool FDocumentSettings::operator==(const FDocumentSettings& RHS) const
{
	return (DimensionType == RHS.DimensionType) &&
		(DimensionUnit == RHS.DimensionUnit) &&
		(MinimumDistanceIncrement == RHS.MinimumDistanceIncrement) &&
		(FMath::IsNearlyEqual(Latitude, RHS.Latitude, 0.001f)) &&
		(FMath::IsNearlyEqual(Longitude, RHS.Longitude, 0.001f)) &&
		(FMath::IsNearlyEqual(TrueNorthDegree, RHS.TrueNorthDegree, 0.001f));
}

bool FDocumentSettings::operator!=(const FDocumentSettings& RHS) const
{
	return !(*this == RHS);
}


FDocumentSettingDelta::FDocumentSettingDelta(const FDocumentSettings& InPreviousSettings, const FDocumentSettings& InNextSettings)
	: PreviousSettings(InPreviousSettings)
	, NextSettings(InNextSettings)
{
}

bool FDocumentSettingDelta::ApplyTo(UModumateDocument* Document, UWorld* World) const
{
	return Document->ApplySettingsDelta(*this, World);
}

FDeltaPtr FDocumentSettingDelta::MakeInverse() const
{
	auto inverse = MakeShared<FDocumentSettingDelta>();
	inverse->PreviousSettings = NextSettings;
	inverse->NextSettings = PreviousSettings;
	return inverse;
}

FStructDataWrapper FDocumentSettingDelta::SerializeStruct()
{
	return FStructDataWrapper(StaticStruct(), this, true);
}
