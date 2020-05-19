// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "DocumentManagement/ModumateObjectInstance.h"

namespace Modumate
{
	class MODUMATE_API FModumateControlPointObjectBase : public FModumateObjectInstanceImplBase
	{
	protected:
		TArray<FVector> NormalControlPoints;
		FVector AnchorLocation, NormalLocation;
		FQuat NormalRotation;

		bool GotGeometry = false;

		void UpdateControlPoints();
		void UpdateNormalization();

		virtual void SetFromDataRecordAndRotation(const FMOIDataRecord &dataRec, const FVector &origin, const FQuat &rotation) override;
		virtual void SetFromDataRecordAndDisplacement(const FMOIDataRecord &dataRec, const FVector &displacement) override;

		FModumateControlPointObjectBase(FModumateObjectInstance *moi);
	};
}