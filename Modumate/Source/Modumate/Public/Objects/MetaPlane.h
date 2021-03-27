// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

#include "MetaPlane.generated.h"

UCLASS()
class MODUMATE_API AMOIMetaPlane : public AMOIPlaneBase
{
	GENERATED_BODY()
public:
	AMOIMetaPlane();

	virtual bool GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupDynamicGeometry() override;

protected:
	void UpdateCachedGraphData();
	virtual float GetAlpha() const override;

	TArray<int32> LastChildIDs;
};

