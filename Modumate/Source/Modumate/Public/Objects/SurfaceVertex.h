// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/VertexBase.h"

class AVertexActor;

class MODUMATE_API FMOISurfaceVertexImpl : public FMOIVertexImplBase
{
public:
	FMOISurfaceVertexImpl(FModumateObjectInstance *moi);

	virtual FVector GetLocation() const override;
	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetTangents(TArray<FVector>& OutTangents) const override;

protected:
	FVector CachedDeprojectedLocation;
};
