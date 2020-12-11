// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/VertexBase.h"

class AVertexActor;

class MODUMATE_API AMOISurfaceVertex : public FMOIVertexImplBase
{
public:
	AMOISurfaceVertex();

	virtual FVector GetLocation() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetTangents(TArray<FVector>& OutTangents) const override;

protected:
	FVector CachedDeprojectedLocation;
};
