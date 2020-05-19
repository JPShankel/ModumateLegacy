// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceGraphObjects.h"

namespace Modumate
{
	void FMOIGraphVertex::SetLocation(const FVector &p)
	{
		if (ensureAlways(MOI))
		{
			MOI->SetControlPoints(TArray<FVector>());
			MOI->AddControlPoint(p);
		}
	}

	FVector FMOIGraphVertex::GetLocation() const
	{
		if (ensureAlways(MOI && (MOI->GetControlPoints().Num() == 1)))
		{
			return MOI->GetControlPoint(0);
		}

		return FVector::ZeroVector;
	}
}
