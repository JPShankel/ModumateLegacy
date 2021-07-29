// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateCore/ModumateTypes.h"
#include "MOIStructureData.generated.h"

UENUM(BlueprintType) 
enum class EPointType : uint8
{
	Corner,
	Middle,
};

USTRUCT(BlueprintType) 
struct FStructurePoint
{
	GENERATED_BODY()

	FStructurePoint() {}
	FStructurePoint(const FVector& point, const FVector& dir, 
					int32 cp1, int32 cp2 = -1, int32 objID = MOD_ID_NONE,
					EPointType type = EPointType::Corner)
		: ObjID(objID)
		, Point(point)
		, Direction(dir)
		, CP1(cp1), CP2(cp2)
		, PointType(type)
	{ }

	UPROPERTY()
	int32 ObjID = MOD_ID_NONE;

	UPROPERTY()
	FVector Point = FVector::ZeroVector;

	UPROPERTY()
	FVector Direction = FVector::ZeroVector;

	UPROPERTY()
	int32 CP1 = INDEX_NONE;

	UPROPERTY()
	int32 CP2 = INDEX_NONE;

	UPROPERTY()
	EPointType PointType = EPointType::Corner;
};

USTRUCT(BlueprintType) 
struct FStructureLine
{
	GENERATED_BODY()

	FStructureLine() {}
	FStructureLine(const FVector& p1, const FVector& p2, int32 cp1, int32 cp2, int32 objID = MOD_ID_NONE)
		: ObjID(objID)
		, P1(p1), P2(p2)
		, CP1(cp1), CP2(cp2)
	{ }

	UPROPERTY()
	int32 ObjID = MOD_ID_NONE;

	UPROPERTY()
	FVector P1 = FVector::ZeroVector;

	UPROPERTY()
	FVector P2 = FVector::ZeroVector;

	UPROPERTY()
	int32 CP1 = 0;

	UPROPERTY()
	int32 CP2 = 0;
};
