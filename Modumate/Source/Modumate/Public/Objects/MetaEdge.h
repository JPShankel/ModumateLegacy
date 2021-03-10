// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/EdgeBase.h"
#include "Objects/MiterNode.h"
#include "UI/Properties/InstPropWidgetEdgeDetail.h"

#include "MetaEdge.generated.h"

UCLASS()
class MODUMATE_API AMOIMetaEdge : public AMOIEdgeBase, public IMiterNode
{
	GENERATED_BODY()

public:
	AMOIMetaEdge();

	virtual bool CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas) override;
	virtual void ShowAdjustmentHandles(AEditModelPlayerController* Controller, bool bShow) override;
	virtual void RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) override;
	virtual const IMiterNode* GetMiterInterface() const override { return this; }

	// Begin IMiterNode interface
	virtual const FMiterData &GetMiterData() const;
	// End IMiterNode interface

	UPROPERTY()
	class AMOIEdgeDetail* CachedEdgeDetailMOI;

protected:
	FMiterData CachedMiterData;
	FGuid CachedEdgeDetailDataID;
	uint32 CachedEdgeDetailConditionHash;
	FColor BaseDefaultColor, BaseGroupedColor, HoverDefaultColor, HoverGroupedColor;

	void ResetEdgeDetail();
	void CacheEdgeDetail();
};
