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
	FString name;

	UPROPERTY()
	EWebMOIPropertyType type = EWebMOIPropertyType::text;

	UPROPERTY()
	TArray<FString> valueArray;

	UPROPERTY()
	FString displayName;

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
	FString id;

	UPROPERTY()
	FString displayName;
};

USTRUCT()
struct MODUMATE_API FWebMOIAlignmentTarget
{
	GENERATED_BODY()

	UPROPERTY()
	int32 moiId;
	
	UPROPERTY()
	FGuid presetId;

	UPROPERTY()
	FString displayName;

	UPROPERTY()
	TArray<FWebMOIZone> zones;
};

USTRUCT()
struct MODUMATE_API FWebMOI
{
	GENERATED_BODY()

	UPROPERTY()
	int32 id = 0;

	UPROPERTY()
	FString displayName;

	UPROPERTY()
	TArray<int32> children;

	UPROPERTY()
	FGuid presetId;

	UPROPERTY()
	int32 parent = 0;

	UPROPERTY()
	EObjectType type = EObjectType::OTNone;

	UPROPERTY()
	bool isVisible = true;

	UPROPERTY()
	TMap<FString, FWebMOIProperty> properties;

	friend bool operator==(const FWebMOI& Lhs, const FWebMOI& RHS)
	{
		return Lhs.id == RHS.id
			&& Lhs.displayName == RHS.displayName
			&& Lhs.presetId == RHS.presetId
			&& Lhs.parent == RHS.parent
			&& Lhs.type == RHS.type
			&& Lhs.isVisible == RHS.isVisible
			&& Lhs.children == RHS.children;
	}

	friend bool operator!=(const FWebMOI& Lhs, const FWebMOI& RHS)
	{
		return !(Lhs == RHS);
	}

	UPROPERTY()
	FMOIAlignment alignment;

	UPROPERTY()
	TArray<FWebMOIZone> zones;

	UPROPERTY()
	TArray<FWebMOIAlignmentTarget> alignmentTargets;
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