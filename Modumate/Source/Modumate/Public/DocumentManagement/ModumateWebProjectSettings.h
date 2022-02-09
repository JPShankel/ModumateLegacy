// Copyright 2022 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ModumateWebProjectSettings.generated.h"

USTRUCT()
struct MODUMATE_API FModumateWebProjectSettingsProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString value;

	UPROPERTY()
	TMap<FString, FString> options;

	UPROPERTY()
	TArray<int32> range;
};

USTRUCT()
struct MODUMATE_API FModumateWebProjectSettings
{
	GENERATED_BODY()

	UPROPERTY()
	FModumateWebProjectSettingsProperty units;

	UPROPERTY()
	FModumateWebProjectSettingsProperty increment;

	UPROPERTY()
	FModumateWebProjectSettingsProperty latitude;

	UPROPERTY()
	FModumateWebProjectSettingsProperty longitude;

	UPROPERTY()
	FModumateWebProjectSettingsProperty trueNorth;

	UPROPERTY()
	FModumateWebProjectSettingsProperty microphone;

	UPROPERTY()
	FModumateWebProjectSettingsProperty speaker;

	UPROPERTY()
	FModumateWebProjectSettingsProperty shadows;

	UPROPERTY()
	FModumateWebProjectSettingsProperty antiAliasing;

	UPROPERTY()
	FModumateWebProjectSettingsProperty version;
};