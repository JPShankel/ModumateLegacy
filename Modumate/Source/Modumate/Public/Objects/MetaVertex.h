// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/VertexBase.h"

class AModumateVertexActor_CPP;

class MODUMATE_API FMOIMetaVertexImpl : public FMOIVertexImplBase
{
public:
	FMOIMetaVertexImpl(FModumateObjectInstance *moi);

	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
	virtual void GetTangents(TArray<FVector>& OutTangents) const override;
};