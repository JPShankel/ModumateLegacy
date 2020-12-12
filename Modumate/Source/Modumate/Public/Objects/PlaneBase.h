// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "PlaneBase.generated.h"

UCLASS()
class MODUMATE_API AMOIPlaneBase : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIPlaneBase();

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override;
	virtual FVector GetNormal() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const override;
	virtual void SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller) override;
	virtual bool ShowStructureOnSelection() const override;
	virtual void GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled) override;

	virtual bool OnSelected(bool bIsSelected) override;
	virtual bool OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered) override;
	virtual void PostCreateObject(bool bNewObject) override;

protected:
	virtual float GetAlpha() const;
	virtual void UpdateMaterial();
	virtual void UpdateConnectedVisuals();

	FArchitecturalMaterial MaterialData;
	FColor SelectedColor, HoveredColor, BaseColor;
	TArray<FVector> CachedPoints;
	FPlane CachedPlane;
	FVector CachedAxisX, CachedAxisY, CachedOrigin, CachedCenter;
	TArray<FPolyHole3D> CachedHoles;
	TArray<AModumateObjectInstance*> TempConnectedMOIs;
};
