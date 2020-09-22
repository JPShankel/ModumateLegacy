// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateCore/ModumateTypes.h"


struct FStructurePoint
{
	FStructurePoint() {}
	FStructurePoint(const FVector& point, const FVector& dir, int32 cp1, int32 cp2 = -1, int32 objID = MOD_ID_NONE)
		: ObjID(objID)
		, Point(point)
		, Direction(dir)
		, CP1(cp1), CP2(cp2)
	{ }

	int32 ObjID = MOD_ID_NONE;
	FVector Point = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	int32 CP1 = INDEX_NONE;
	int32 CP2 = INDEX_NONE;
};

struct FStructureLine
{
	FStructureLine() {}
	FStructureLine(const FVector& p1, const FVector& p2, int32 cp1, int32 cp2, int32 objID = MOD_ID_NONE)
		: ObjID(objID)
		, P1(p1), P2(p2)
		, CP1(cp1), CP2(cp2)
	{ }

	int32 ObjID = MOD_ID_NONE;
	FVector P1 = FVector::ZeroVector, P2 = FVector::ZeroVector;
	int32 CP1 = 0, CP2 = 0;
};
