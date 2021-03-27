// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

#include "SurfacePolygon.generated.h"

UCLASS()
class MODUMATE_API AMOISurfacePolygon : public AMOIPlaneBase
{
	GENERATED_BODY()
public:
	AMOISurfacePolygon();

	virtual bool GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

protected:
	TArray<FVector> CachedOffsetPoints;
	TArray<FPolyHole3D> CachedOffsetHoles;
	FTransform CachedTransform;
	bool bInteriorPolygon;
	bool bInnerBoundsPolygon;

	virtual float GetAlpha() const override;
};
