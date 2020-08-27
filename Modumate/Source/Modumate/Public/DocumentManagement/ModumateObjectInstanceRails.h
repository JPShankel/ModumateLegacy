// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/ModumateDynamicObjectBase.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"

class MODUMATE_API FMOIRailImpl : public FDynamicModumateObjectInstanceImpl
{
public:
	FMOIRailImpl(FModumateObjectInstance *moi);
	virtual ~FMOIRailImpl();

	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool EnableHover) override {};
	virtual TArray<FModelDimensionString> GetDimensionStrings() const override;
};

