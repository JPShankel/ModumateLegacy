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

class MODUMATE_API FMOISurfaceGraphImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOISurfaceGraphImpl(FModumateObjectInstance *moi);

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual FVector GetNormal() const override;
	virtual void GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	bool CheckGraphLink();
	bool IsGraphLinked() const { return bLinked; }

	static constexpr float VisualNormalOffset = 0.1f;

protected:
	bool UpdateCachedGraphData();

	TMap<int32, int32> FaceIdxToVertexID;
	TArray<FVector> CachedFacePoints;
	FTransform CachedFaceOrigin;

	FMOISurfaceGraphData InstanceData;

	bool bLinked = true;
};

