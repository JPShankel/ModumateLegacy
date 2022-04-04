// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ModumateCore/EdgeDetailData.h"
#include "Objects/EdgeBase.h"
#include "Objects/MiterNode.h"
#include "UI/Properties/InstPropWidgetEdgeDetail.h"

#include "MetaEdge.generated.h"

USTRUCT()
struct MODUMATE_API FMOIMetaEdgeData
{
	GENERATED_BODY()

	FMOIMetaEdgeData();

	UPROPERTY()
	bool FlipDirection = false;

	UPROPERTY()
	float CalculatedLength = 0.0f;
};

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
	virtual bool FromWebMOI(const FString& InJson) override;
	virtual bool ToWebMOI(FWebMOI& OutMOI) const override;

	// Begin IMiterNode interface
	virtual const FMiterData &GetMiterData() const;
	// End IMiterNode interface

	uint32 GetDetailConditionHash() const { return CachedEdgeDetailConditionHash; }
	float CalculateLength(AEditModelPlayerController* AEMPlayerController) const;

	UPROPERTY()
	class AMOIEdgeDetail* CachedEdgeDetailMOI;

	UPROPERTY()
	FMOIMetaEdgeData InstanceData;

protected:
	FMiterData CachedMiterData;
	FEdgeDetailData CachedEdgeDetailData;
	FGuid CachedEdgeDetailDataID;
	uint32 CachedEdgeDetailConditionHash;
	FColor BaseDefaultColor, BaseGroupedColor, HoverDefaultColor, HoverGroupedColor;

	void ResetEdgeDetail();
	void CacheEdgeDetail();

	UFUNCTION()
	void OnInstPropUIChangedCycle(int32 BasisValue);
};
