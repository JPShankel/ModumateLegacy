// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "Graph/ModumateGraph.h"
#include "Graph/ModumateGraph3DTypes.h"

class AAdjustmentHandleActor_CPP;
class ALineActor;

namespace Modumate
{
	class MODUMATE_API FMOIRoofPerimeterImpl : public FModumateObjectInstanceImplBase
	{
	public:
		FMOIRoofPerimeterImpl(FModumateObjectInstance *moi);

		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual FQuat GetRotation() const override { return FQuat::Identity; }
		virtual FVector GetCorner(int32 index) const override;
		virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
		void SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller);
		virtual void ClearAdjustmentHandles(AEditModelPlayerController_CPP *Controller) override;
		virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *Controller, bool bShow) override;
		virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool bIsHovered) override;
		virtual void OnSelected(bool bNewSelected) override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual AActor *RestoreActor() override;
		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual bool ShowStructureOnSelection() const override { return false; }
		virtual bool UseStructureDataForCollision() const override { return true; }

	protected:
		TWeakObjectPtr<UWorld> World;
		bool bValidPerimeterLoop;
		TArray<FSignedID> CachedEdgeIDs;
		TArray<FVector> CachedPerimeterPoints;
		FVector CachedPerimeterCenter;
		FGraph CachedPerimeterGraph;
		FPlane CachedPlane;

		TSet<FTypedGraphObjID> TempGroupMembers, TempConnectedGraphIDs;
		TArray<FVector2D> TempPerimeterPoints2D;

		TWeakObjectPtr<AActor> PerimeterActor;
		TMap<int32, TWeakObjectPtr<ALineActor>> LineActors;
		TSet<int32> TempEdgeIDsToAdd, TempEdgeIDsToRemove;

		TWeakObjectPtr<AAdjustmentHandleActor_CPP> CreateFacesHandleActor;

		bool UpdatePerimeterIDs();
		void UpdatePerimeterGeometry();
		void UpdateLineActors();
	};
}
