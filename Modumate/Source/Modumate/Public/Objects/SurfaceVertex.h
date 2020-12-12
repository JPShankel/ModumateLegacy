// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/VertexBase.h"

#include "SurfaceVertex.generated.h"

class AVertexActor;

UCLASS()
class MODUMATE_API AMOISurfaceVertex : public AMOIVertexBase
{
	GENERATED_BODY()
public:
	AMOISurfaceVertex();

	virtual FVector GetLocation() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetTangents(TArray<FVector>& OutTangents) const override;

protected:
	FVector CachedDeprojectedLocation;
};
