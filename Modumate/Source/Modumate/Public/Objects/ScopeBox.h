// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectInstance.h"

class MODUMATE_API FMOIScopeBoxImpl : public FModumateObjectInstance
{
public:
	FMOIScopeBoxImpl();

	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
	virtual void PreDestroy() override;

	virtual void SetupDynamicGeometry() override;
	virtual void UpdateDynamicGeometry() override;

	virtual FVector GetCorner(int32 index) const override;
	virtual FVector GetNormal() const override;

	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	virtual bool ShowStructureOnSelection() const override;
	virtual void AddDraftingLines(UHUDDrawWidget *HUDDrawWidget) override;

	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;

protected:
	FArchitecturalMaterial MaterialData;

	FLinearColor EdgeSelectedColor;
	FLinearColor EdgeColor;

	FVector Normal;

	// Adjustment handle variables
	// TODO: should any of these be exposed, or are they the same every time
	FVector HandleScale;
	float PolyPointHandleOffset;
	float ExtrusionHandleOffset;
};
