// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/EdgeBase.h"
#include "Objects/MiterNode.h"

class MODUMATE_API FMOIMetaEdgeImpl : public FMOIEdgeImplBase, IMiterNode
{
public:
	FMOIMetaEdgeImpl(FModumateObjectInstance *moi);

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController_CPP* Controller, bool bShow) override;
	virtual const IMiterNode* GetMiterInterface() const override { return this; }

	// Begin IMiterNode interface
	virtual const FMiterData &GetMiterData() const;
	// End IMiterNode interface

protected:
	FMiterData CachedMiterData;
	FColor BaseDefaultColor, BaseGroupedColor, HoverDefaultColor, HoverGroupedColor;
};

