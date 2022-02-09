// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/DocumentSettings.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/ModumateSettingsMenu.h"


// 1/64", 1/32", 1/16", 1/8", 1/4", 1/2", 1", 2", 4", 6", 1'
const TArray<int32> FDocumentSettings::ImperialDistIncrementMultipliers({ 1, 2, 4, 8, 16, 32, 64, 128, 256, 384, 768 });

// 0.5mm, 1mm, 2mm, 5mm, 1cm, 2cm, 5cm, 10cm, 20cm, 50cm, 1m
const TArray<int32> FDocumentSettings::MetricDistIncrementMultipliers({ 1, 2, 4, 10, 20, 40, 100, 200, 400, 1000, 2000 });


bool FDocumentSettings::ToModumateWebProjectSettings(FModumateWebProjectSettings& Settings) const
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
		FString dimensionPrefId = dimensionPrefDisplayString;
		dimensionPrefId.RemoveSpacesInline();
		Settings.units.options.Add(dimensionPrefDisplayString, dimensionPrefId);
	}

	// Build value for units
	EDimensionPreference curDimensionPref;
	if (UModumateSettingsMenu::GetDimensionPrefFromTypes(DimensionType, DimensionUnit, curDimensionPref))
	{
		int32 curDimensionPrefIndex = dimensionPrefEnum->GetIndexByValue((int64)curDimensionPref);
		Settings.units.value = dimensionPrefEnum->GetDisplayNameTextByIndex(curDimensionPrefIndex).ToString();
		Settings.units.value.RemoveSpacesInline();
	}

	// Get current increments
	double minDistIncrement = 0.0;
	double defaultDistIncrement = 0.0;
	TArray<int32> distIncrementMultipliers;
	switch (DimensionType)
	{
	case EDimensionUnits::DU_Imperial:
		minDistIncrement = FDocumentSettings::MinImperialDistIncrementCM;
		distIncrementMultipliers = FDocumentSettings::ImperialDistIncrementMultipliers;
		defaultDistIncrement = distIncrementMultipliers[FDocumentSettings::DefaultImperialMultiplierIdx] * minDistIncrement;
		break;
	case EDimensionUnits::DU_Metric:
		minDistIncrement = FDocumentSettings::MinMetricDistIncrementCM;
		distIncrementMultipliers = FDocumentSettings::MetricDistIncrementMultipliers;
		defaultDistIncrement = distIncrementMultipliers[FDocumentSettings::DefaultMetricMultiplierIdx] * minDistIncrement;
		break;
	}

	// Build value and options for increments
	Settings.increment.value = FString::SanitizeFloat(defaultDistIncrement);

	for (int32 multiplierIdx = 0; multiplierIdx < distIncrementMultipliers.Num(); ++multiplierIdx)
	{
		int32 distIncrementMultiplier = distIncrementMultipliers[multiplierIdx];
		double distIncrement = minDistIncrement * distIncrementMultiplier;
		FText distIncrementText = UModumateDimensionStatics::CentimetersToDisplayText(distIncrement, 1, DimensionType, DimensionUnit);
		FString distIncrementString = distIncrementText.ToString();
		FString distIncrementId = distIncrementString;
		distIncrementId.RemoveSpacesInline();
		Settings.increment.options.Add(distIncrementString, distIncrementId);

		if (distIncrement == MinimumDistanceIncrement)
		{
			Settings.increment.value = distIncrementString;
			Settings.increment.value.RemoveSpacesInline();
		}
	}

	Settings.latitude.value = FString::SanitizeFloat(Latitude);
	Settings.longitude.value = FString::SanitizeFloat(Longitude);
	Settings.trueNorth.value = FString::SanitizeFloat(TrueNorthDegree);

	return true;
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
