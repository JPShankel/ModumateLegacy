// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "EdgeHosted.generated.h"

USTRUCT()
struct MODUMATE_API FMOIEdgeHostedData
{
	GENERATED_BODY()

	FMOIEdgeHostedData();

	UPROPERTY()
	FVector FlipSigns = FVector::OneVector;

	UPROPERTY()
	FDimensionOffset OffsetUp;

	UPROPERTY()
	FDimensionOffset OffsetNormal;

	UPROPERTY()
	float Rotation = 0.f;
};

UCLASS()
class MODUMATE_API AMOIEdgeHosted : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIEdgeHosted();

	virtual AActor* CreateActor(const FVector& loc, const FQuat& rot) override;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;

	UPROPERTY()
	FMOIEdgeHostedData InstanceData;

protected:

	void InternalUpdateGeometry(bool bCreateCollision);

};
