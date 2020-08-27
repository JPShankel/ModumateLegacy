// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateControlPointObjectBase.h"
#include "Algo/Accumulate.h"

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
		
	if (ensureAlways(NormalControlPoints.Num() == MOI->GetControlPoints().Num()))
	{
		for (int32 i = 0; i < FMath::Min(MOI->GetControlPoints().Num(), NormalControlPoints.Num()); ++i)
		{
			MOI->SetControlPoint(i,NormalLocation + NormalRotation.RotateVector(NormalControlPoints[i] - AnchorLocation));
		}
		UpdateDynamicGeometry();
	}
}

void FModumateControlPointObjectBase::UpdateNormalization()
{
	if (MOI->GetControlPoints().Num() == 0)
	{
		return;
	}

	NormalLocation = Algo::Accumulate(MOI->GetControlPoints(),FVector::ZeroVector,[](const FVector &tot, const FVector &cp){return tot + cp;}) / MOI->GetControlPoints().Num();

	NormalRotation = FQuat::Identity;
	AnchorLocation = NormalLocation;
	NormalControlPoints = MOI->GetControlPoints();
}

void FModumateControlPointObjectBase::SetFromDataRecordAndRotation(const FMOIDataRecordV1 &dataRec, const FVector &origin, const FQuat &rotation)
{
	if (ensure(dataRec.ControlPoints.Num() == MOI->GetControlPoints().Num()))
	{
		for (int32 i = 0; i < dataRec.ControlPoints.Num(); ++i)
		{
			MOI->SetControlPoint(i,origin + rotation.RotateVector(dataRec.ControlPoints[i] - origin));
		}
		UpdateNormalization();
		UpdateDynamicGeometry();
	}
}

void FModumateControlPointObjectBase::SetFromDataRecordAndDisplacement(const FMOIDataRecordV1 &dataRec, const FVector &displacement)
{
	if (ensure(dataRec.ControlPoints.Num() == MOI->GetControlPoints().Num()))
	{
		for (int32 i = 0; i < dataRec.ControlPoints.Num(); ++i)
		{
			MOI->SetControlPoint(i,dataRec.ControlPoints[i] + displacement);
		}
		UpdateNormalization();
		UpdateDynamicGeometry();
	}
}

