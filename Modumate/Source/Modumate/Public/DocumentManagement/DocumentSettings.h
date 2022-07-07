// Copyright 2021 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DocumentManagement/DocumentDelta.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/WebProjectSettings.h"

#include "DocumentSettings.generated.h"


USTRUCT()
struct MODUMATE_API FDocumentSettings
{
	GENERATED_BODY()

	FDocumentSettings();

	static constexpr double MinImperialDistIncrementCM = 0.015625 * UModumateDimensionStatics::InchesToCentimeters;
	static constexpr double MinMetricDistIncrementCM = 0.05;

	static const TArray<int32> ImperialDistIncrementMultipliers;
	static const TArray<int32> MetricDistIncrementMultipliers;

	static constexpr int32 DefaultImperialMultiplierIdx = 3;
	static constexpr int32 DefaultMetricMultiplierIdx = 3;

	UPROPERTY()
	EDimensionUnits DimensionType = EDimensionUnits::DU_Imperial;

	UPROPERTY()
	EUnit DimensionUnit = EUnit::Unspecified;

	UPROPERTY()
	double MinimumDistanceIncrement = 0.0;

	UPROPERTY()
	float Latitude = 45.f;

	UPROPERTY()
	float Longitude = -73.f;

	UPROPERTY()
	float TrueNorthDegree = 0.f;

	UPROPERTY()
	float Number = 0.f;

	UPROPERTY()
	FString Name = TEXT("Name");

	UPROPERTY()
	FString Address = TEXT("Address");

	UPROPERTY()
	FString Description = TEXT("Description");

	bool operator==(const FDocumentSettings& RHS) const;
	bool operator!=(const FDocumentSettings& RHS) const;

	bool ToWebProjectSettings(FWebProjectSettings& OutSettings) const;
	bool FromWebProjectSettings(const FWebProjectSettings& InSettings);
};

template<>
struct TStructOpsTypeTraits<FDocumentSettings> : public TStructOpsTypeTraitsBase2<FDocumentSettings>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};


USTRUCT()
struct MODUMATE_API FDocumentSettingDelta : public FDocumentDelta
{
	GENERATED_BODY()

	FDocumentSettingDelta() = default;
	FDocumentSettingDelta(const FDocumentSettings& InPreviousSettings, const FDocumentSettings& InNextSettings);

	virtual ~FDocumentSettingDelta() {}

	virtual bool ApplyTo(UModumateDocument* doc, UWorld* world) const override;
	virtual FDeltaPtr MakeInverse() const override;
	virtual FStructDataWrapper SerializeStruct() override;

	UPROPERTY()
	FDocumentSettings PreviousSettings;

	UPROPERTY()
	FDocumentSettings NextSettings;
};
