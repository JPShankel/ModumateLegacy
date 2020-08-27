// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateDynamicObjectBase.h"

class MODUMATE_API FMOIFlatPolyImpl : public FDynamicModumateObjectInstanceImpl
{
public:
	FMOIFlatPolyImpl(FModumateObjectInstance *moi, bool wantsInvertHandle = true);
	virtual ~FMOIFlatPolyImpl();

	bool WantsInvertHandle;

	virtual FVector GetCorner(int32 index) const override;
	float CalcThickness(const FBIMAssemblySpec &Assembly) const;
	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller);
	virtual TArray<FModelDimensionString> GetDimensionStrings() const override;
};
