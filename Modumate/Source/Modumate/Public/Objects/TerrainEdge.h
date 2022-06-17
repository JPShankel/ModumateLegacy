// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/EdgeBase.h"

#include "TerrainEdge.generated.h"

USTRUCT()
struct MODUMATE_API FMOITerrainEdgeData
{
	GENERATED_BODY();

	UPROPERTY()
	FGuid ID;
};

UCLASS()
class MODUMATE_API AMOITerrainEdge : public AMOIEdgeBase
{
	GENERATED_BODY()

public:
	AMOITerrainEdge();

	virtual FVector GetCorner(int32 Index) const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	FMOITerrainEdgeData InstanceData;

protected:
	FVector CachedPoints[2];
};
