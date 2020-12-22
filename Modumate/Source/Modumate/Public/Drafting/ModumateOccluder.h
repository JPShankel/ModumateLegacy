// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "MatrixTypes.h"

class FModumateOccluder
{
public:
	FModumateOccluder(const FVector3d& A, const FVector3d& B, const FVector3d& C);
	bool operator<(const FModumateOccluder& rhs) const
		{ return Area2D < rhs.Area2D; };
	bool operator>(const FModumateOccluder& rhs) const
		{ return Area2D > rhs.Area2D; }
	FVector3d Vertices[3];
	FMatrix2d BarycentricMx;
	FBox2D BoundingBox;
	double MinZ;
	double Area2D;

	double DepthAtPoint(FVector2d Point) const;
	double DepthAtPoint(FVector3d Point) const;
	bool IsWithinTriangle(FVector2d Point) const;
	bool IsWithinTriangle(FVector3d Point) const;
};
