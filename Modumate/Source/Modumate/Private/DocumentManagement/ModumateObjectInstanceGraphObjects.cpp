// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceGraphObjects.h"

namespace Modumate
{
	void FMOIGraphVertex::SetLocation(const FVector &p)
	{
		if (ensureAlways(MOI))
		{
			MOI->ControlPoints.SetNum(1);
			MOI->ControlPoints[0] = p;
		}
	}

	FVector FMOIGraphVertex::GetLocation() const
	{
		if (ensureAlways(MOI && (MOI->ControlPoints.Num() == 1)))
		{
			return MOI->ControlPoints[0];
		}

		return FVector::ZeroVector;
	}
}
