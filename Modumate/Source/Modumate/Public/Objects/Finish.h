// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/DynamicMeshActor.h"

class AEditModelPlayerController;

class AModumateObjectInstance;

#include "Finish.generated.h"

USTRUCT()
struct MODUMATE_API FMOIFinishData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D FlipSigns = FVector2D::UnitVector;

	UPROPERTY()
	int32 OverrideOriginIndex = INDEX_NONE;

	UPROPERTY()
	float RotationOffset = 0.0f;
};

UCLASS()
class MODUMATE_API AMOIFinish : public AModumateObjectInstance
{
	GENERATED_BODY()
public:
	virtual void PreDestroy() override;
	virtual FVector GetCorner(int32 index) const override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController* controller) override;

	virtual bool GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const override;

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
		const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
		TArray<TArray<FVector>>& OutPerimeters) const override;
	bool ProcessQuantities(FQuantitiesVisitor& QuantitiesVisitor) const override;

	UPROPERTY()
	FMOIFinishData InstanceData;

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
