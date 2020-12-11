// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/VertexBase.h"

class AVertexActor;

class MODUMATE_API AMOIMetaVertex : public FMOIVertexImplBase
{
public:
	AMOIMetaVertex();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetTangents(TArray<FVector>& OutTangents) const override;
};
