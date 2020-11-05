// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"

class AEditModelPlayerController_CPP;

class FModumateObjectInstance;

class MODUMATE_API FMOIFinishImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOIFinishImpl(FModumateObjectInstance *moi);
	virtual ~FMOIFinishImpl();

	virtual void Destroy() override;
	virtual FVector GetCorner(int32 index) const override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller) override;

protected:
	void UpdateConnectedEdges();
	void MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags DirtyFlags);

	FTransform CachedGraphOrigin;
	TArray<FVector> CachedPerimeter;
	TArray<FPolyHole3D> CachedHoles;
	TArray<int32> CachedConnectedEdgeIDs;
	TArray<FModumateObjectInstance*> CachedConnectedEdgeChildren;
};
