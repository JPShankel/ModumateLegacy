// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WebMOI.generated.h"

UENUM()
enum class EWebMOIPropertyType : uint8
{
	none = 0,
	text,
	boolean,
	color,
	moiId,
	offset,
	flip,
	button,
	eulerRotation3D,
	quatRotation,
	extension,
	terrainMaterial,
	height
};

USTRUCT()
struct MODUMATE_API FWebMOIProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	EWebMOIPropertyType Type = EWebMOIPropertyType::text;

	UPROPERTY()
	TArray<FString> ValueArray;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	bool isVisible;

	UPROPERTY()
	bool isEditable;
};

USTRUCT()
struct MODUMATE_API FWebMOI
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ID = 0;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	TArray<int32> Children;

	UPROPERTY()
	FGuid PresetID;

	UPROPERTY()
	int32 Parent = 0;

	UPROPERTY()
	EObjectType Type = EObjectType::OTNone;

	UPROPERTY()
	bool isVisible = true;

	UPROPERTY()
	TMap<FString, FWebMOIProperty> Properties;
};
