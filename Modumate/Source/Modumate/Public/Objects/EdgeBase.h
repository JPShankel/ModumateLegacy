// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

class UWorld;
class ALineActor;

class MODUMATE_API FMOIEdgeImplBase : public AModumateObjectInstance
{
public:
	FMOIEdgeImplBase();

	virtual FVector GetLocation() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual bool OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered) override;
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
	virtual bool OnSelected(bool bIsSelected) override;
	virtual void GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual bool ShowStructureOnSelection() const override { return false; }
	virtual bool UseStructureDataForCollision() const override { return true; }

protected:
	float GetThicknessMultiplier() const;
	void UpdateMaterial();

	TWeakObjectPtr<UWorld> World;
	TArray<AModumateObjectInstance*> CachedConnectedMOIs;
	TWeakObjectPtr<ALineActor> LineActor;
	FColor SelectedColor, HoveredColor, BaseColor;
	float HoverThickness, SelectedThickness;
};

