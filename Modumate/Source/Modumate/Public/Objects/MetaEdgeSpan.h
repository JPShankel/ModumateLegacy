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
	virtual void SetupDynamicGeometry() override;

	UPROPERTY()
	FMOIMetaEdgeSpanData InstanceData;

	static void MakeMetaEdgeSpanDeltaPtr(UModumateDocument* Doc, int32 NewID, const TArray<int32>& InMemberObjects, TSharedPtr<FMOIDelta>& OutMoiDeltaPtr);
	static void MakeMetaEdgeSpanDeltaFromGraph(FGraph3D* InGraph, int32 NewID, const TArray<int32>& InMemberObjects, TArray<FDeltaPtr>& OutDeltaPtrs);

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	const FGraph3DEdge* GetCachedGraphEdge() const { return CachedGraphEdge; }

protected:

	bool TryUpdateCachedGraphData();

	FGraph3DEdge* CachedGraphEdge = nullptr;
};