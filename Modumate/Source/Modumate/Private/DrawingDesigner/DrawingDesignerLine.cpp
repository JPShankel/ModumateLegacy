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

float FDrawingDesignerLine::Length() const
{
	return FVector::Distance(P1, P2);
}

float FDrawingDesignerLine::GetDDThickness() const
{
	return Thickness * ThicknessScale;
}

FColor FDrawingDesignerLine::GetLineShadeAsColor() const
{
	const uint8 component = uint8(FMath::Clamp(GreyValue * 255.0f + 0.5f, 0.0f, 255.0f));
	return FColor(component, component, component);
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

void FDrawingDesignerLined::Canonicalize()
{
	if (LessThan(P2, P1))
	{
		Swap(P1, P2);
	}
}

double FDrawingDesignerLined::Length() const
{
	return (P2 - P1).Length();
}

bool FDrawingDesignerLined::operator<(const FDrawingDesignerLined& Rhs) const
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

FDrawingDesignerLined::operator FDrawingDesignerLine() const
{
	FDrawingDesignerLine retVal((FVector)P1, (FVector)P2, (FVector)N);
	retVal.Thickness = float(Thickness);
	return retVal;
}

bool FDrawingDesignerLined::operator==(const FDrawingDesignerLined& Rhs) const
{
	return ((P1 == Rhs.P1 && P2 == Rhs.P2) || (P1 == Rhs.P2 && P2 == Rhs.P1));
}

bool FDrawingDesignerLined::operator!=(const FDrawingDesignerLined& Rhs) const
{
	return !operator==(Rhs);
}

bool FDrawingDesignerLined::LessThan(const FVector3d& A, const FVector3d& B)
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
