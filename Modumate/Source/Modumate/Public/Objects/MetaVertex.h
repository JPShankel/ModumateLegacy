// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/VertexBase.h"

#include "MetaVertex.generated.h"

class AVertexActor;

UCLASS()
class MODUMATE_API AMOIMetaVertex : public AMOIVertexBase
{
	GENERATED_BODY()
public:
	AMOIMetaVertex();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetTangents(TArray<FVector>& OutTangents) const override;
};
