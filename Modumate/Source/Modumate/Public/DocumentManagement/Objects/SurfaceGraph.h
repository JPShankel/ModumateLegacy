// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateObjectInstance.h"

class MODUMATE_API FMOISurfaceGraphImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOISurfaceGraphImpl(FModumateObjectInstance *moi);

	virtual void SetRotation(const FQuat &r) override;
	virtual FQuat GetRotation() const override;
	virtual void SetLocation(const FVector &p) override;
	virtual FVector GetLocation() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;

protected:
	bool UpdateCachedGraphData();

	TArray<FVector> CachedFacePoints, PrevFacePoints;
	FTransform PrevFaceOrigin, CachedFaceOrigin;
};

