// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "CoreMinimal.h"

class AStaticMeshActor;
class AAdjustmentHandleActor;

class FModumateObjectInstance;

class MODUMATE_API FMOIFFEImpl : public FModumateObjectInstanceImplBase
{
protected:
	TWeakObjectPtr<UWorld> World;
	TArray<TWeakObjectPtr<AAdjustmentHandleActor>> AdjustmentHandles;
	FVector CachedLocation;
	FQuat CachedRotation;
	FVector CachedFaceNormal;
	TArray<int32> CachedFaceIndices;

public:

	FMOIFFEImpl(FModumateObjectInstance *moi);
	virtual ~FMOIFFEImpl();
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
	virtual void SetRotation(const FQuat &r) override;
	virtual FQuat GetRotation() const override;
	virtual void SetLocation(const FVector &p) override;
	virtual FVector GetLocation() const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *Controller) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;

protected:
	void InternalUpdateGeometry();
};