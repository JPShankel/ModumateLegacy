// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "DocumentManagement/ModumateObjectInstance.h"

class MODUMATE_API FModumateControlPointObjectBase : public FModumateObjectInstanceImplBase
{
protected:
	TArray<FVector> NormalControlPoints;
	FVector AnchorLocation, NormalLocation;
	FQuat NormalRotation;

	bool GotGeometry = false;

	void UpdateControlPoints();
	void UpdateNormalization();

	FModumateControlPointObjectBase(FModumateObjectInstance *moi);
};
