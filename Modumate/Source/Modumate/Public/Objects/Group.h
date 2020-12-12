// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

class AMOIGroupActor_CPP;

class AModumateObjectInstance;

#include "Group.generated.h"

UCLASS()
class MODUMATE_API AMOIGroup : public AModumateObjectInstance
{
	GENERATED_BODY()
private:
	FVector CachedLocation;
	FVector CachedExtents;

	// Only used temporarily inside of GetStructuralPointsAndLines
	mutable TArray<FStructurePoint> TempPoints;
	mutable TArray<FStructureLine> TempLines;

public:
	AMOIGroup();
	virtual ~AMOIGroup() {};

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;

	virtual AActor *RestoreActor() override;
	virtual AActor *CreateActor(const FVector &loc, const FQuat &rot) override;

	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping = false, bool bForSelection = false) const override;

	static float StructuralExtentsExpansion;
};
