// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

class MODUMATE_API AMOIMetaPlane : public FMOIPlaneImplBase
{
public:
	AMOIMetaPlane();

	virtual void GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupDynamicGeometry() override;

protected:
	void UpdateCachedGraphData();
	virtual float GetAlpha() const override;

	TArray<int32> LastChildIDs;
};

