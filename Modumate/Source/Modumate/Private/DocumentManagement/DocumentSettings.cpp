// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/DocumentSettings.h"
#include "DocumentManagement/ModumateDocument.h"


// 1/64", 1/32", 1/16", 1/8", 1/4", 1/2", 1", 2", 4", 6", 1'
const TArray<int32> FDocumentSettings::ImperialDistIncrementMultipliers({ 1, 2, 4, 8, 16, 32, 64, 128, 256, 384, 768 });

// 0.5mm, 1mm, 2mm, 5mm, 1cm, 2cm, 5cm, 10cm, 20cm, 50cm, 1m
const TArray<int32> FDocumentSettings::MetricDistIncrementMultipliers({ 1, 2, 4, 10, 20, 40, 100, 200, 400, 1000, 2000 });


FDocumentSettings::FDocumentSettings()
{
	MinimumDistanceIncrement = ImperialDistIncrementMultipliers[DefaultImperialMultiplierIdx] * MinImperialDistIncrementCM;
}

bool FDocumentSettings::operator==(const FDocumentSettings& RHS) const
{
	return (DimensionType == RHS.DimensionType) &&
		(DimensionUnit == RHS.DimensionUnit) &&
		(MinimumDistanceIncrement == RHS.MinimumDistanceIncrement);
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
