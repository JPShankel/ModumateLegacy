// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/Objects/VertexBase.h"

class AModumateVertexActor_CPP;

class MODUMATE_API FMOISurfaceVertexImpl : public FMOIVertexImplBase
{
public:
	FMOISurfaceVertexImpl(FModumateObjectInstance *moi);

	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
};
