// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "VectorTypes.h"

// Represents a line in view-space - x, y are position in final image,
// z is depth from projection plane.
class FModumateViewLineSegment
{
public:
	FModumateViewLineSegment() = default;
	FModumateViewLineSegment(FVector3d LineStart, FVector3d LineEnd, int32 LinePosition = 0)
	{
		Start = LineStart; End = LineEnd; Position = LinePosition;
	}
	FVector3d AsVector() const { return End - Start; }
	double Length() const { return AsVector().Length(); }
	FVector2d AsVector2() const { return { End.X - Start.X, End.Y - Start.Y }; }
	double Length2() const { return AsVector2().Length(); }

	FVector3d Start;
	FVector3d End;
	int32 Position = 0;
};
