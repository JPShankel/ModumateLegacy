// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "EdgeBase.generated.h"

class UWorld;
class ALineActor;

UCLASS()
class MODUMATE_API AMOIEdgeBase : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIEdgeBase();

	virtual FVector GetLocation() const override;
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual bool OnHovered(AEditModelPlayerController *controller, bool bIsHovered) override;
	virtual AActor *CreateActor(const FVector &loc, const FQuat &rot) override;
	virtual bool OnSelected(bool bIsSelected) override;
	virtual bool GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual bool ShowStructureOnSelection() const override { return false; }
	virtual bool UseStructureDataForCollision() const override { return true; }
	virtual void PreDestroy() override;

	void UpdateLineArrowVisual();

protected:
	float GetThicknessMultiplier() const;
	void UpdateMaterial();

	TArray<AModumateObjectInstance*> CachedConnectedMOIs;
	TWeakObjectPtr<ALineActor> LineActor;
	FColor SelectedColor, HoveredColor, BaseColor;
	float HoverThickness, SelectedThickness;
	bool CacheIsSelected = false;

	UPROPERTY()
	class UStaticMeshComponent* LineArrowCylinderMesh;

	UPROPERTY()
	class UMaterialInstanceDynamic* ArrowDynMat;
};

