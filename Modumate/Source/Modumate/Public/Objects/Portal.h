// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "CoreMinimal.h"

class AAdjustmentHandleActor;

class FModumateObjectInstance;
class MODUMATE_API FMOIPortalImpl : public FModumateObjectInstanceImplBase
{
protected:
	TArray<TWeakObjectPtr<AAdjustmentHandleActor>> AdjustmentHandles;
	TWeakObjectPtr<AEditModelPlayerController_CPP> Controller;

	void SetControlPointsFromAssembly();
	void UpdateAssemblyFromControlPoints();
	void SetupCompoundActor();
	bool SetRelativeTransform(const FVector2D &InRelativePos, const FQuat &InRelativeRot);
	bool CacheCorners();
	void GetFarDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane &Plane, const FBox2D& BoundingBox) const;

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
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;

	virtual void OnAssemblyChanged() override;
	virtual FVector GetNormal() const;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual TArray<FModelDimensionString> GetDimensionStrings() const override;
	virtual void TransverseObject() override;

	static void GetControlPointsFromAssembly(const FBIMAssemblySpec &ObjectAssembly, TArray<FVector> &ControlPoints);

	virtual void GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const override;

	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;
};