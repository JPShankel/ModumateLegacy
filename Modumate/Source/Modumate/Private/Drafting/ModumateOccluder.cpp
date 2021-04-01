// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateOccluder.h"

FModumateOccluder::FModumateOccluder(const FVec3d& A, const FVec3d& B, const FVec3d& C)
{
	Vertices[0] = A;
	Vertices[1] = B;
	Vertices[2] = C;

	FVec2d u((double*)(B - A));
	FVec2d v((double*)(C - A));
	double d = u.Cross(v);
	if (d < 0.0)
	{	// Ensure right handed
		Swap(Vertices[1], Vertices[2]);
		Swap(u, v);
		d = -d;
	}
	Area2D = 0.5 * d;

	BarycentricMx.Row0 = { v.Y / d , -v.X / d };
	BarycentricMx.Row1 = { -u.Y / d, u.X / d };

	static constexpr double boxExpansion = 0.5;

	BoundingBox = FBox2D({ FVector2D(FVector(A)), FVector2D(FVector(B)), FVector2D(FVector(C)) });
	BoundingBox.ExpandBy(boxExpansion);
	MinZ = FMath::Min3(Vertices[0].Z, Vertices[1].Z, Vertices[2].Z) - boxExpansion;
}

double FModumateOccluder::DepthAtPoint(FVec2d Point) const
{
	FVec2d alphaBeta = BarycentricMx * (Point - FVec2d((const double*)(Vertices[0])) );
	return (1.0 - alphaBeta.X - alphaBeta.Y) * Vertices[0].Z + alphaBeta.X * Vertices[1].Z + alphaBeta.Y * Vertices[2].Z;
}

double FModumateOccluder::DepthAtPoint(FVec3d Point) const
{
	return DepthAtPoint(FVec2d((double*) Point));
}

bool FModumateOccluder::IsWithinTriangle(FVec2d Point) const
{
	FVec2d alphaBeta = BarycentricMx * (Point - FVec2d((const double*)(Vertices[0])));
	return FMath::IsWithin(alphaBeta.X, 0.0, 1.0) && FMath::IsWithin(alphaBeta.Y, 0.0, 1.0)
		&& alphaBeta.X + alphaBeta.Y < 1.0;
}

bool FModumateOccluder::IsWithinTriangle(FVec3d Point) const
{
	return IsWithinTriangle(FVec2d((double*)Point));
}
