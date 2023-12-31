// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/PlaneBase.h"

#include "TerrainPolygon.generated.h"

USTRUCT()
struct MODUMATE_API FMOITerrainPolygonData
{
	GENERATED_BODY();

	UPROPERTY()
	FGuid ID;
};

UCLASS()
class MODUMATE_API AMOITerrainPolygon : public AMOIPlaneBase
{
	GENERATED_BODY()

public:
	AMOITerrainPolygon();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController*) override { };

	UPROPERTY()
	FMOITerrainPolygonData InstanceData;

protected:
	bool CreateMaterialMoi(TArray<FDeltaPtr>* SideEffectDeltas);

	FTransform CachedTransform;
};
