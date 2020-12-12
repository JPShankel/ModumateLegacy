// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "VertexBase.generated.h"

class AVertexActor;

UCLASS()
class MODUMATE_API AMOIVertexBase : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIVertexBase();

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override { return FQuat::Identity; }
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual void GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled) override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual AActor *CreateActor(const FVector &loc, const FQuat &rot) override;
	virtual bool OnSelected(bool bIsSelected) override;
	virtual bool ShowStructureOnSelection() const override { return false; }
	virtual bool UseStructureDataForCollision() const override { return true; }
	virtual void GetTangents(TArray<FVector>& OutTangents) const;

protected:
	TWeakObjectPtr<AVertexActor> VertexActor;
	TArray<AModumateObjectInstance*> CachedConnectedMOIs;

	FColor SelectedColor, BaseColor;
	float DefaultHandleSize;
	float SelectedHandleSize;
};
