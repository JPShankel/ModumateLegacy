// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/PlaneBase.h"

#include "TerrainPolygon.generated.h"

UCLASS()
class MODUMATE_API AMOITerrainPolygon : public AMOIPlaneBase
{
	GENERATED_BODY()

public:
	AMOITerrainPolygon();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController* controller) override { };

protected:
};
