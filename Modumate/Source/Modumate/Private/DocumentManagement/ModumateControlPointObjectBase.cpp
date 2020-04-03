// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateControlPointObjectBase.h"
#include "Algo/Accumulate.h"

namespace Modumate
{
	FModumateControlPointObjectBase::FModumateControlPointObjectBase(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, AnchorLocation(ForceInitToZero)
		, NormalLocation(ForceInitToZero)
		, NormalRotation(ForceInitToZero)
		, GotGeometry(false)
	{}

	void FModumateControlPointObjectBase::UpdateControlPoints()
	{
		if (NormalControlPoints.Num() == 0)
		{
			return;
		}
		
		if (ensureAlways(NormalControlPoints.Num() == MOI->ControlPoints.Num()))
		{
			for (int32 i = 0; i < FMath::Min(MOI->ControlPoints.Num(), NormalControlPoints.Num()); ++i)
			{
				MOI->ControlPoints[i] = NormalLocation + NormalRotation.RotateVector(NormalControlPoints[i] - AnchorLocation);
			}
			UpdateDynamicGeometry();
		}
	}

	void FModumateControlPointObjectBase::UpdateNormalization()
	{
		if (MOI->ControlPoints.Num() == 0)
		{
			return;
		}

		NormalLocation = Algo::Accumulate(MOI->ControlPoints,FVector::ZeroVector,[](const FVector &tot, const FVector &cp){return tot + cp;}) / MOI->ControlPoints.Num();

		NormalRotation = FQuat::Identity;
		AnchorLocation = NormalLocation;
		NormalControlPoints = MOI->ControlPoints;
	}

	void FModumateControlPointObjectBase::SetFromDataRecordAndRotation(const FMOIDataRecordV1 &dataRec, const FVector &origin, const FQuat &rotation)
	{
		if (ensure(dataRec.ControlPoints.Num() == MOI->ControlPoints.Num()))
		{
			for (int32 i = 0; i < dataRec.ControlPoints.Num(); ++i)
			{
				MOI->ControlPoints[i] = origin + rotation.RotateVector(dataRec.ControlPoints[i] - origin);
			}
			UpdateNormalization();
			UpdateDynamicGeometry();
		}
	}

	void FModumateControlPointObjectBase::SetFromDataRecordAndDisplacement(const FMOIDataRecordV1 &dataRec, const FVector &displacement)
	{
		if (ensure(dataRec.ControlPoints.Num() == MOI->ControlPoints.Num()))
		{
			for (int32 i = 0; i < dataRec.ControlPoints.Num(); ++i)
			{
				MOI->ControlPoints[i] = dataRec.ControlPoints[i] + displacement;
			}
			UpdateNormalization();
			UpdateDynamicGeometry();
		}
	}

}

