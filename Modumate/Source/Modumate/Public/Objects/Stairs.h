// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"
#include "Objects/LayeredObjectInterface.h"
#include "CoreMinimal.h"

class AEditModelPlayerController_CPP;
class FModumateObjectInstance;

class MODUMATE_API FMOIStaircaseImpl : public FModumateObjectInstanceImplBase
{
public:
	FMOIStaircaseImpl(FModumateObjectInstance *moi);
	virtual ~FMOIStaircaseImpl();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller) override;

	virtual void SetupDynamicGeometry() override;

protected:
	bool bCachedUseRisers, bCachedStartRiser, bCachedEndRiser;
	TArray<TArray<FVector>> CachedTreadPolys;
	TArray<TArray<FVector>> CachedRiserPolys;
	TArray<FVector> CachedRiserNormals;
	TArray<FLayerGeomDef> TreadLayers;
	TArray<FLayerGeomDef> RiserLayers;
	FCachedLayerDimsByType CachedTreadDims;
	FCachedLayerDimsByType CachedRiserDims;
	float TreadRun = 0.0f;
};
