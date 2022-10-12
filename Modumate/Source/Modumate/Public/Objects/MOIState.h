// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/ModumateObjectEnums.h"
#include "ModumateCore/StructDataWrapper.h"

#include "MOIState.generated.h"

UENUM()
enum class EZoneOrigin : uint8
{
	None = 0,
	Center,
	Back,
	Front,
	MassingPlane
};

USTRUCT()
struct MODUMATE_API FMOIZoneDesignator
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MoiId = 0;

	UPROPERTY()
	FString ZoneID;

	UPROPERTY()
	EZoneOrigin ZoneOrigin = EZoneOrigin::Center;
};

USTRUCT()
struct MODUMATE_API FMOIZoneOffset
{
	GENERATED_BODY()

	UPROPERTY()
	float Displacement = 0.0f;

	UPROPERTY()
	FMOIZoneDesignator ZoneDesignator;
};

USTRUCT()
struct MODUMATE_API FMOIPresetZonePlane
{
	GENERATED_BODY()

	UPROPERTY()
	int32 moiId = 0;

	UPROPERTY()
	FString zoneId;

	UPROPERTY()
	EZoneOrigin origin = EZoneOrigin::Center;

	UPROPERTY()
	FGuid presetGuid;

	UPROPERTY()
	float displacement = 0.0f;

	bool operator==(const FMOIPresetZonePlane& Other) const;
	bool operator!=(const FMOIPresetZonePlane& Other) const;
};

USTRUCT()
struct MODUMATE_API FMOIAlignment
{
	GENERATED_BODY()

	UPROPERTY()
	FString displayName = TEXT("Alignment Preset");

	UPROPERTY()
	FMOIPresetZonePlane subjectPZP;
	
	UPROPERTY()
	FMOIPresetZonePlane targetPZP;
	
	bool operator==(const FMOIAlignment& Other) const;
	bool operator!=(const FMOIAlignment& Other) const;

	FString ToString() const;
	void FromString(FString& str);
};

USTRUCT()
struct MODUMATE_API FMOIStateData
{
	GENERATED_BODY()

	FMOIStateData();
	FMOIStateData(int32 InID, EObjectType InObjectType, int32 InParentID = MOD_ID_NONE);

	UPROPERTY()
	int32 ID = MOD_ID_NONE;

	UPROPERTY()
	EObjectType ObjectType = EObjectType::OTNone;

	UPROPERTY()
	int32 ParentID = MOD_ID_NONE;

	UPROPERTY()
	FGuid AssemblyGUID;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FStructDataWrapper CustomData;

	UPROPERTY()
	FMOIAlignment Alignment;

	bool operator==(const FMOIStateData& Other) const;
	bool operator!=(const FMOIStateData& Other) const;
};
