// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

class AModumateVertexActor_CPP;

class MODUMATE_API FMOIVertexImplBase : public FModumateObjectInstanceImplBase
{
public:
	FMOIVertexImplBase(FModumateObjectInstance *moi);

	virtual FVector GetLocation() const override;
	virtual FQuat GetRotation() const override { return FQuat::Identity; }
	virtual FVector GetCorner(int32 index) const override;
	virtual int32 GetNumCorners() const override;
	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual AActor *CreateActor(UWorld *world, const FVector &loc, const FQuat &rot) override;
	virtual void OnSelected(bool bNewSelected) override;
	virtual bool ShowStructureOnSelection() const override { return false; }
	virtual bool UseStructureDataForCollision() const override { return true; }
	virtual void GetTangents(TArray<FVector>& OutTangents) const = 0;

protected:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AModumateVertexActor_CPP> VertexActor;
	TArray<FModumateObjectInstance*> CachedConnectedMOIs;

	float DefaultHandleSize;
	float SelectedHandleSize;
};
