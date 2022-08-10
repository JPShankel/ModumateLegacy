// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "BIMKernel/Core/BIMKey.h"
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
	int32 MoiId = 0;

	UPROPERTY()
	FString ZoneID;

	UPROPERTY()
	EZoneOrigin Origin = EZoneOrigin::Center;

	UPROPERTY()
	FGuid PresetGUID;

	UPROPERTY()
	float Displacement = 0.0f;

	bool operator==(const FMOIPresetZonePlane& Other) const;
	bool operator!=(const FMOIPresetZonePlane& Other) const;
};

USTRUCT()
struct MODUMATE_API FMOIAlignment
{
	GENERATED_BODY()

	UPROPERTY()
	FString DisplayName = TEXT("Alignment Preset");

	UPROPERTY()
	FMOIPresetZonePlane SubjectPZP;
	
	UPROPERTY()
	FMOIPresetZonePlane TargetPZP;
	
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
