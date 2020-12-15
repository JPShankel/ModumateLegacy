// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateOccluder.h"

FModumateOccluder::FModumateOccluder(const FVector3d& A, const FVector3d& B, const FVector3d& C)
{
	Vertices[0] = A;
	Vertices[1] = B;
	Vertices[2] = C;

	FVector2d u((double*)(B - A));
	FVector2d v((double*)(C - A));
	double d = u.Cross(v);
	if (d < 0.0)
	{	// Ensure right handed
		Swap(Vertices[1], Vertices[2]);
		Swap(u, v);
		d = -d;
	}

	BarycentricMx.Row0 = { v.Y / d , -v.X / d };
	BarycentricMx.Row1 = { -u.Y / d, u.X / d };

	static constexpr double boxExpansion = 0.5;

	BoundingBox = FBox2D({ FVector2D(FVector(A)), FVector2D(FVector(B)), FVector2D(FVector(C)) });
	BoundingBox.ExpandBy(boxExpansion);
	MinZ = FMath::Min3(Vertices[0].Z, Vertices[1].Z, Vertices[2].Z);
}

double FModumateOccluder::Area2D() const
{
	return FMath::Abs(0.5 * FVector2d((double*)(Vertices[1] - Vertices[0])).Cross(FVector2d((double*)(Vertices[2] - Vertices[0])) ));
}

double FModumateOccluder::DepthAtPoint(FVector2d Point) const
{
	FVector2d alphaBeta = BarycentricMx * (Point - FVector2d((const double*)(Vertices[0])) );
	return (1.0 - alphaBeta.X - alphaBeta.Y) * Vertices[0].Z + alphaBeta.X * Vertices[1].Z + alphaBeta.Y * Vertices[2].Z;
}

double FModumateOccluder::DepthAtPoint(FVector3d Point) const
{
	return DepthAtPoint(FVector2d((double*) Point));
}

bool FModumateOccluder::IsWithinTriangle(FVector2d Point) const
{
	FVector2d alphaBeta = BarycentricMx * (Point - FVector2d((const double*)(Vertices[0])));
	return FMath::IsWithin(alphaBeta.X, 0.0, 1.0) && FMath::IsWithin(alphaBeta.Y, 0.0, 1.0)
		&& alphaBeta.X + alphaBeta.Y < 1.0;
}

bool FModumateOccluder::IsWithinTriangle(FVector3d Point) const
{
	return IsWithinTriangle(FVector2d((double*)Point));
}
