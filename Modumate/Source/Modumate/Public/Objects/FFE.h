// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "FFE.generated.h"

class AStaticMeshActor;
class AAdjustmentHandleActor;

USTRUCT()
struct MODUMATE_API FMOIFFEData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FQuat Rotation = FQuat::Identity;

	UPROPERTY()
	bool bLateralInverted = false;
};

UCLASS()
class MODUMATE_API AMOIFFE : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIFFE();

	virtual AActor *CreateActor(const FVector &loc, const FQuat &rot) override;
	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController *Controller) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void SetIsDynamic(bool bIsDynamic) override;
	virtual bool GetIsDynamic() const override;
	virtual bool GetInvertedState(FMOIStateData& OutState) const override;
	virtual bool GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const override;
	virtual bool ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const override;

	UPROPERTY()
	FMOIFFEData InstanceData;

protected:
	void InternalUpdateGeometry();

	TArray<TWeakObjectPtr<AAdjustmentHandleActor>> AdjustmentHandles;
	FVector CachedLocation = FVector::ZeroVector;
	FQuat CachedRotation = FQuat::Identity;
	FVector CachedFaceNormal = FVector::ZeroVector;
	TArray<int32> CachedFaceIndices;

	virtual void UpdateQuantities() override;
};
