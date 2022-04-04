// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

#include "MetaPlane.generated.h"

USTRUCT()
struct MODUMATE_API FMOIMetaPlaneData
{
	GENERATED_BODY()

	FMOIMetaPlaneData();

	UPROPERTY()
	bool FlipDirection = false;

	UPROPERTY()
	double CalculatedArea = 0;
};

UCLASS()
class MODUMATE_API AMOIMetaPlane : public AMOIPlaneBase
{
	GENERATED_BODY()
public:
	AMOIMetaPlane();

	virtual bool GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupDynamicGeometry() override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;
	virtual bool ToWebMOI(FWebMOI& OutMOI) const override;
	virtual bool FromWebMOI(const FString& InJson) override;

	UPROPERTY()
	FMOIMetaPlaneData InstanceData;

protected:
	void UpdateCachedGraphData();
	virtual float GetAlpha() const override;

	UFUNCTION()
	void OnInstPropUIChangedCycle(int32 BasisValue);

	TArray<int32> LastChildIDs;
};

