// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/EdgeBase.h"

#include "MetaEdgeSpan.generated.h"

USTRUCT()
struct MODUMATE_API FMOIMetaEdgeSpanData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> GraphMembers;
};

UCLASS()
class MODUMATE_API AMOIMetaEdgeSpan : public AMOIEdgeBase
{
	GENERATED_BODY()
public:
	AMOIMetaEdgeSpan();

	virtual FVector GetLocation() const override;
	virtual FVector GetCorner(int32 index) const override;

	UPROPERTY()
	FMOIMetaEdgeSpanData InstanceData;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	const FGraph3DEdge* GetCachedGraphEdge() const { return CachedGraphEdge; }

protected:

	bool TryUpdateCachedGraphData();

	FGraph3DEdge* CachedGraphEdge = nullptr;
};