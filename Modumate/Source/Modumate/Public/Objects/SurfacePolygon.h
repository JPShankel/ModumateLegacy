// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

class MODUMATE_API FMOISurfacePolygonImpl : public FMOIPlaneImplBase
{
public:
	FMOISurfacePolygonImpl(FModumateObjectInstance *moi);

	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

protected:
	TArray<FVector> CachedOffsetPoints;
	TArray<FPolyHole3D> CachedOffsetHoles;
	FTransform CachedTransform;
	bool bInteriorPolygon;
	bool bInnerBoundsPolygon;

	virtual float GetAlpha() const override;
};
