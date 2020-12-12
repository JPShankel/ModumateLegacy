// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/EdgeBase.h"

#include "SurfaceEdge.generated.h"

UCLASS()
class MODUMATE_API AMOISurfaceEdge : public AMOIEdgeBase
{
	GENERATED_BODY()
public:
	AMOISurfaceEdge();

	virtual FVector GetCorner(int32 index) const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

protected:
	FVector CachedDeprojectedStart, CachedDeprojectedEnd;
};
