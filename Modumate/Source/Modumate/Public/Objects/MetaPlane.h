// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/PlaneBase.h"

class MODUMATE_API FMOIMetaPlaneImpl : public FMOIPlaneImplBase
{
public:
	FMOIMetaPlaneImpl(FModumateObjectInstance *moi);

	virtual void PostCreateObject(bool bNewObject) override;
	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual void SetupDynamicGeometry() override;
	virtual void OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered) override;
	virtual void OnSelected(bool bIsSelected) override;

protected:
	void UpdateConnectedVisuals();
	void UpdateCachedGraphData();
	virtual float GetAlpha() const override;

	TArray<FModumateObjectInstance *> TempConnectedMOIs;
	TArray<int32> LastChildIDs;
};

