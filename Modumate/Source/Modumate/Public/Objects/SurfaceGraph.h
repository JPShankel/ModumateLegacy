// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "SurfaceGraph.generated.h"

USTRUCT()
struct MODUMATE_API FMOISurfaceGraphData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ParentFaceIndex;
};

UCLASS()
class MODUMATE_API AMOISurfaceGraph : public AModumateObjectInstance
{
	GENERATED_BODY()
public:
	AMOISurfaceGraph();

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FVector GetNormal() const override;
	virtual void PreDestroy() override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	bool CheckGraphLink();
	bool IsGraphLinked() const { return bLinked; }

	static constexpr float VisualNormalOffset = 0.1f;

	UPROPERTY()
	FMOISurfaceGraphData InstanceData;

protected:
	bool UpdateCachedGraphData();
	bool CalculateFaces(const TArray<int32>& AddIDs, TMap<int32, TArray<FVector2D>>& OutPolygonsToAdd, TMap<int32, TArray<int32>>& OutFaceToVertices);

	TMap<int32, int32> FaceIdxToVertexID;
	TMap<int32, int32> GraphFaceToInnerBound;
	TMap<int32, int32> GraphVertexToBoundVertex;

	TArray<FVector> CachedFacePoints;
	FTransform CachedFaceOrigin;

	bool bLinked = true;
};

