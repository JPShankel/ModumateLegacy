// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetEdgeDetail.generated.h"


UENUM()
enum class EEdgeDetailWidgetActions : uint8
{
	None,
	Create,
	Swap,
	Edit
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropEdgeDetailButtonPress, EEdgeDetailWidgetActions, EdgeDetailAction);

UCLASS()
class MODUMATE_API UInstPropWidgetEdgeDetail : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* DetailName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonCreate;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonSwap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonEdit;

	TSet<int32> CurrentEdgeIDs;
	TSet<FGuid> CurrentPresetValues;
	uint32 CurrentConditionValue;

	UPROPERTY()
	FOnInstPropEdgeDetailButtonPress ButtonPressedEvent;

	void RegisterValue(UObject* Source, int32 EdgeID, const FGuid& DetailPresetID, uint32 DetailConditionHash);

	static bool TryMakeUniquePresetDisplayName(const struct FBIMPresetCollection& PresetCollection, const struct FEdgeDetailData& NewDetailData, FText& OutDisplayName);

protected:
	virtual void BroadcastValueChanged() override;
	bool OnCreateOrSwap(FGuid NewDetailPresetID);

	UFUNCTION()
	void OnClickedCreate();

	UFUNCTION()
	void OnClickedSwap();

	UFUNCTION()
	void OnClickedEdit();

	EEdgeDetailWidgetActions LastClickedAction = EEdgeDetailWidgetActions::None;
};
