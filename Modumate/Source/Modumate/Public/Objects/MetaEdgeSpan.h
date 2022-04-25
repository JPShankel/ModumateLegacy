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

	using FInstanceData = FMOIMetaEdgeSpanData;

	virtual FVector GetCorner(int32 index) const override;

	UPROPERTY()
	FMOIMetaEdgeSpanData InstanceData;

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	const FGraph3DEdge* GetCachedGraphEdge() const { return &CachedGraphEdge; }

	virtual void SetupDynamicGeometry() override;

	virtual void PreDestroy() override;

protected:

	bool UpdateCachedEdge();

	FGraph3DEdge CachedGraphEdge;
};