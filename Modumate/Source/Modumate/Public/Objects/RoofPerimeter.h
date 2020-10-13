// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph3DTypes.h"

class AAdjustmentHandleActor;
class ACreateRoofFacesHandle;
class AEditRoofEdgeHandle;
class ALineActor;
class ARetractRoofFacesHandle;
class URoofPerimeterPropertiesWidget;

class MODUMATE_API FMOIRoofPerimeterImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOIRoofPerimeterImpl(FModumateObjectInstance *moi);

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override { return FQuat::Identity; }
	virtual FVector GetCorner(int32 index) const override;
	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow) override;
	virtual void OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered) override;
	virtual void OnSelected(bool bIsSelected) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual AActor *RestoreActor() override;
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
	virtual FVector GetNormal() const override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool ShowStructureOnSelection() const override { return false; }
	virtual bool UseStructureDataForCollision() const override { return true; }

protected:
	TWeakObjectPtr<UWorld> World;
	bool bValidPerimeterLoop;
	TArray<FGraphSignedID> CachedEdgeIDs;
	TArray<int32> CachedFaceIDs;
	TArray<FVector> CachedPerimeterPoints;
	FVector CachedPerimeterCenter;
	TSharedPtr<Modumate::FGraph2D> CachedPerimeterGraph;
	FPlane CachedPlane;

	TSet<int32> TempGroupMembers, TempGroupEdges, TempGroupFaces, TempConnectedGraphIDs;
	TArray<FVector2D> TempPerimeterPoints2D;

	TWeakObjectPtr<AActor> PerimeterActor;
	TMap<int32, TWeakObjectPtr<ALineActor>> LineActors;
	TSet<int32> TempEdgeIDsToAdd, TempEdgeIDsToRemove;

	bool bAdjustmentHandlesVisible = false;
	TWeakObjectPtr<ACreateRoofFacesHandle> CreateFacesHandle;
	TWeakObjectPtr<ARetractRoofFacesHandle> RetractFacesHandle;
	TWeakObjectPtr<URoofPerimeterPropertiesWidget> DefaultPropertiesWidget;
	TMap<int32, TWeakObjectPtr<AEditRoofEdgeHandle>> EdgeHandlesByID;

	bool UpdateConnectedIDs();
	void UpdatePerimeterGeometry();
	void UpdateLineActors();
};

