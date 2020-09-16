// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateDynamicObjectBase.h"
#include "CoreMinimal.h"

class AEditModelPlayerController_CPP;
class FModumateObjectInstance;

class MODUMATE_API FMOIStaircaseImpl : public FDynamicModumateObjectInstanceImpl
{
public:
	FMOIStaircaseImpl(FModumateObjectInstance *moi);
	virtual ~FMOIStaircaseImpl();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override { }
	virtual TArray<FModelDimensionString> GetDimensionStrings() const override;

protected:
	bool bCachedUseRisers, bCachedStartRiser, bCachedEndRiser;
	TArray<TArray<FVector>> CachedTreadPolys;
	TArray<TArray<FVector>> CachedRiserPolys;
	TArray<FVector> CachedRiserNormals;
};

