// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"

class AEditModelPlayerController_CPP;

class AModumateObjectInstance;

class MODUMATE_API AMOIFinish : public AModumateObjectInstance
{
public:
	virtual void PreDestroy() override;
	virtual FVector GetCorner(int32 index) const override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller) override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;

protected:
	void UpdateConnectedEdges();
	void MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags DirtyFlags);

	void GetBeyondLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const;
	void GetInPlaneLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const;

	FTransform CachedGraphOrigin;
	TArray<FVector> CachedPerimeter;
	TArray<FPolyHole3D> CachedHoles;
	TArray<int32> CachedConnectedEdgeIDs;
	TArray<AModumateObjectInstance*> CachedConnectedEdgeChildren;
};
