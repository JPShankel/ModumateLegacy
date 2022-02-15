// Copyright 2022 Modumate, Inc,  All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WebProjectSettings.generated.h"

USTRUCT()
struct MODUMATE_API FWebProjectSettingsPropertyOption
{
	GENERATED_BODY()

	FWebProjectSettingsPropertyOption();
	FWebProjectSettingsPropertyOption(const FString& InName, const FString& InId);

	UPROPERTY()
	FString name;

	UPROPERTY()
	FString id;
};

USTRUCT()
struct MODUMATE_API FWebProjectSettingsProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString value;

	UPROPERTY()
	TArray<FWebProjectSettingsPropertyOption> options;

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