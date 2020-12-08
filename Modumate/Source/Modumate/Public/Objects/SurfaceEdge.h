// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/EdgeBase.h"

class MODUMATE_API FMOISurfaceEdgeImpl : public FMOIEdgeImplBase
{
public:
	FMOISurfaceEdgeImpl(FModumateObjectInstance *moi);

	virtual FVector GetCorner(int32 index) const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

protected:
	FVector CachedDeprojectedStart, CachedDeprojectedEnd;
};
