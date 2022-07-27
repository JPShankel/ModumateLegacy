// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectEnums.h"
#include "Objects/MOIState.h"
#include "WebMOI.generated.h"

UENUM()
enum class EWebMOIPropertyType : uint8
{
	none = 0,
	text,
	number,
	label,
	boolean,
	color,
	moiId,
	offset,
	flip2DYZ,
	flip2DXY,
	flip3D,
	flipDirection,
	button,
	eulerRotation3D,
	quatRotation,
	extension,
	terrainMaterial,
	height,
	cameraDate,
	cameraTime,
	cameraPositionUpdate,
	edgeDetailHash,
	edgeDetail,
	size
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
struct MODUMATE_API FWebMOIZone
{
	GENERATED_BODY()

	UPROPERTY()
	FString ID;

	UPROPERTY()
	FString DisplayName;
};

USTRUCT()
struct MODUMATE_API FWebMOIAlignmentTarget
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MoiID;
	
	UPROPERTY()
	FGuid PresetID;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	TArray<FWebMOIZone> Zones;
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

	friend bool operator==(const FWebMOI& Lhs, const FWebMOI& RHS)
	{
		return Lhs.ID == RHS.ID
			&& Lhs.DisplayName == RHS.DisplayName
			&& Lhs.PresetID == RHS.PresetID
			&& Lhs.Parent == RHS.Parent
			&& Lhs.Type == RHS.Type
			&& Lhs.isVisible == RHS.isVisible
			&& Lhs.Children == RHS.Children;
	}

	friend bool operator!=(const FWebMOI& Lhs, const FWebMOI& RHS)
	{
		return !(Lhs == RHS);
	}

	UPROPERTY()
	FMOIAlignment Alignment;

	UPROPERTY()
	TArray<FWebMOIZone> Zones;

	UPROPERTY()
	TArray<FWebMOIAlignmentTarget> AlignmentTargets;
};

USTRUCT()
struct MODUMATE_API FWebMOIPackage
{
	GENERATED_BODY()

	UPROPERTY()
	EObjectType objectType = EObjectType::OTNone;

	UPROPERTY()
	TArray<FWebMOI> mois;
};