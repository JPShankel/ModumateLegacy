// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ModumateObjectInstance.h"
#include "CoreMinimal.h"

class AAdjustmentHandleActor_CPP;

namespace Modumate
{
	class FModumateObjectInstance;
	class MODUMATE_API FMOIPortalImpl : public FModumateObjectInstanceImplBase
	{
	protected:
		TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>> AdjustmentHandles;
		TWeakObjectPtr<AEditModelPlayerController_CPP> Controller;

		void SetControlPointsFromAssembly();
		void UpdateAssemblyFromControlPoints();
		void SetupCompoundActor();
		bool SetRelativeTransform(const FVector2D &InRelativePos, const FQuat &InRelativeRot);
		bool CacheCorners();

		FVector2D CachedRelativePos;
		FVector CachedWorldPos;
		FQuat CachedRelativeRot, CachedWorldRot;
		bool bHaveValidTransform;

		TArray<FVector> CachedCorners;
	public:

		FMOIPortalImpl(FModumateObjectInstance *moi);
		virtual ~FMOIPortalImpl();

		virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
		virtual void SetRotation(const FQuat &r) override;
		virtual FQuat GetRotation() const override;
		virtual void SetLocation(const FVector &p) override;
		virtual FVector GetLocation() const override;
		virtual void SetWorldTransform(const FTransform &NewTransform) override;
		virtual FTransform GetWorldTransform() const override;
		virtual FVector GetCorner(int32 index) const override;
		virtual void ClearAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
		virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller);
		virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP *controller, bool show) override;
		virtual void GetAdjustmentHandleActors(TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>>& outHandleActors) override;

		virtual void OnAssemblyChanged() override;
		virtual FVector GetNormal() const;
		virtual bool CleanObject(EObjectDirtyFlags DirtyFlag) override;
		virtual void SetupDynamicGeometry() override;
		virtual void UpdateDynamicGeometry() override;
		virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
		virtual TArray<FModelDimensionString> GetDimensionStrings() const override;
		virtual void SetFromDataRecordAndRotation(const FMOIDataRecord &dataRec, const FVector &origin, const FQuat &rotation) override;
		virtual void SetFromDataRecordAndDisplacement(const FMOIDataRecord &dataRec, const FVector &displacement) override;
		virtual void InvertObject() override;
		virtual void TransverseObject() override;
		virtual FModumateWallMount GetWallMountForSelf(int32 originIndex) const override;
		virtual void SetWallMountForSelf(const FModumateWallMount &wm) override;

		static void GetControlPointsFromAssembly(const FModumateObjectAssembly &ObjectAssembly, TArray<FVector> &ControlPoints);

		virtual void GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override;
	};
}