// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "MatrixTypes.h"

class FModumateOccluder
{
public:
	FModumateOccluder(const FVec3d& A = FVec3d::Zero(), const FVec3d& B = FVec3d::Zero(), const FVec3d& C = FVec3d::Zero());
	bool operator<(const FModumateOccluder& rhs) const
		{ return Area2D < rhs.Area2D; };
	bool operator>(const FModumateOccluder& rhs) const
		{ return Area2D > rhs.Area2D; }
	FVec3d Vertices[3];
	FMatrix2d BarycentricMx;
	FBox2D BoundingBox;
	double MinZ;
	double Area2D;

	double DepthAtPoint(FVec2d Point) const;
	double DepthAtPoint(FVec3d Point) const;
	bool IsWithinTriangle(FVec2d Point) const;
	bool IsWithinTriangle(FVec3d Point) const;
};
