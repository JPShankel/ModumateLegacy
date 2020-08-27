// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateDynamicObjectBase.h"

class MODUMATE_API FMOIPlaneImplBase : public FDynamicModumateObjectInstanceImpl
{
public:
	FMOIPlaneImplBase(FModumateObjectInstance *moi);

	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FQuat GetRotation() const override;
	virtual void SetRotation(const FQuat& r) override;
	virtual FVector GetLocation() const override;
	virtual FVector GetNormal() const override;
	virtual TArray<FModelDimensionString> GetDimensionStrings() const override;
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

