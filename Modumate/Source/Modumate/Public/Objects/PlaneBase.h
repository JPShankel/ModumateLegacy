// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectInstance.h"

class MODUMATE_API FMOIPlaneImplBase : public FModumateObjectInstanceImplBase
{
public:
	FMOIPlaneImplBase(FModumateObjectInstance *moi);

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual FVector GetNormal() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller);
	virtual bool ShowStructureOnSelection() const override;

protected:
	virtual float GetAlpha() const;

protected:
	FArchitecturalMaterial MaterialData;
	TArray<FVector> CachedPoints;
	FPlane CachedPlane;
	FVector CachedAxisX, CachedAxisY, CachedOrigin, CachedCenter;
	TArray<FPolyHole3D> CachedHoles;
};

