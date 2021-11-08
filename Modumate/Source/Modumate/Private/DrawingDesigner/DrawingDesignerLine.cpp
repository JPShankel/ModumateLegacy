// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerLine.h"

bool FDrawingDesignerLine::operator==(const FDrawingDesignerLine& Rhs) const
{
	return ((P1 == Rhs.P1 && P2 == Rhs.P2) || (P1 == Rhs.P2 && P2 == Rhs.P1)) && (Thickness == Rhs.Thickness);
}

bool FDrawingDesignerLine::operator!=(const FDrawingDesignerLine& Rhs) const
{
	return !operator==(Rhs);
}

void FDrawingDesignerLine::Canonicalize()
{
	if (LessThan(P2, P1))
	{
		Swap(P1, P2);
	}
}

bool FDrawingDesignerLine::operator<(const FDrawingDesignerLine& Rhs) const
{
	if (P1 == Rhs.P1)
	{
		return LessThan(P2, Rhs.P2);
	}
	else
	{
		return LessThan(P1, Rhs.P1);
	}
}

bool FDrawingDesignerLine::LessThan(const FVector& A, const FVector& B)
{
	if (A.X == B.X)
	{
		if (A.Y == B.Y)
		{
			return A.Z < B.Z;
		}
		else
		{
			return A.Y < B.Y;
		}
	}
	else
	{
		return A.X < B.X;
	}
}
