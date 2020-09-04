// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DocumentManagement/Objects/VertexBase.h"

class AModumateVertexActor_CPP;

class MODUMATE_API FMOIMetaVertexImpl : public FMOIVertexImplBase
{
public:
	FMOIMetaVertexImpl(FModumateObjectInstance *moi);

	virtual void GetStructuralPointsAndLines(TArray<FStructurePoint>& OutPoints, TArray<FStructureLine>& OutLines, bool bForSnapping = false, bool bForSelection = false) const override;
	virtual void UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled) override;
	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas) override;
};

