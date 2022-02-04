// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Graph/Graph2D.h"
#include "Graph/Graph3DTypes.h"
#include "Objects/ModumateRoofStatics.h"
#include "Objects/ModumateObjectInstance.h"

#include "RoofPerimeter.generated.h"

class AAdjustmentHandleActor;
class ACreateRoofFacesHandle;
class AEditRoofEdgeHandle;
class ALineActor;
class ARetractRoofFacesHandle;
class URoofPerimeterPropertiesWidget;

UCLASS()
class MODUMATE_API AMOIRoofPerimeter : public AModumateObjectInstance
{
	GENERATED_BODY()
public:
	AMOIRoofPerimeter();

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override { return FQuat::Identity; }
	virtual FVector GetCorner(int32 index) const override;
	virtual bool GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController *Controller) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController *Controller, bool bShow) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual AActor *CreateActor(const FVector &loc, const FQuat &rot) override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool ShowStructureOnSelection() const override { return false; }
	virtual bool UseStructureDataForCollision() const override { return true; }

	const TArray<FGraphSignedID>& GetCachedEdgeIDs() const { return CachedEdgeIDs; }

	UPROPERTY()
	FMOIRoofPerimeterData InstanceData;

protected:
	bool bValidPerimeterLoop;
	TArray<FGraphSignedID> CachedEdgeIDs, PrevCachedEdgeIDs;
	TArray<int32> CachedFaceIDs;
	TArray<FVector> CachedPerimeterPoints;
	FVector CachedPerimeterCenter;
	TSharedPtr<FGraph2D> CachedPerimeterGraph;
	FPlane CachedPlane;

	TSet<int32> TempGroupMembers, TempGroupEdges, TempGroupFaces, TempConnectedGraphIDs;
	TArray<FVector2D> TempPerimeterPoints2D;

	TWeakObjectPtr<AActor> PerimeterActor;

	bool bAdjustmentHandlesVisible = false;
	TWeakObjectPtr<ACreateRoofFacesHandle> CreateFacesHandle;
	TWeakObjectPtr<ARetractRoofFacesHandle> RetractFacesHandle;
	TWeakObjectPtr<URoofPerimeterPropertiesWidget> DefaultPropertiesWidget;
	TMap<int32, TWeakObjectPtr<AEditRoofEdgeHandle>> EdgeHandlesByID;

	bool UpdateConnectedIDs();
	void UpdatePerimeterGeometry();
};

