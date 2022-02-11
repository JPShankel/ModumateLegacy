// Copyright 2022 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WebProjectSettings.generated.h"

USTRUCT()
struct MODUMATE_API FWebProjectSettingsProperty
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
struct MODUMATE_API FWebProjectSettings
{
	GENERATED_BODY()

	UPROPERTY()
	FWebProjectSettingsProperty units;

	UPROPERTY()
	FWebProjectSettingsProperty increment;

	UPROPERTY()
	FWebProjectSettingsProperty latitude;

	UPROPERTY()
	FWebProjectSettingsProperty longitude;

	UPROPERTY()
	FWebProjectSettingsProperty trueNorth;

	UPROPERTY()
	FWebProjectSettingsProperty microphone;

	UPROPERTY()
	FWebProjectSettingsProperty speaker;

	UPROPERTY()
	FWebProjectSettingsProperty shadows;

	UPROPERTY()
	FWebProjectSettingsProperty antiAliasing;

	UPROPERTY()
	FWebProjectSettingsProperty version;
};