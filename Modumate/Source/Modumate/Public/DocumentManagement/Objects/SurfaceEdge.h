// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/Objects/EdgeBase.h"

class MODUMATE_API FMOISurfaceEdgeImpl : public FMOIEdgeImplBase
{
public:
	FMOISurfaceEdgeImpl(FModumateObjectInstance *moi);

	virtual FVector GetCorner(int32 index) const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
	virtual void UpdateVisibilityAndCollision(bool& bOutVisible, bool& bOutCollisionEnabled) override;

protected:
	FVector CachedDeprojectedStart, CachedDeprojectedEnd;
};
