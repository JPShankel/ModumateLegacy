// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetEdgeDetail.generated.h"


UENUM()
enum class EEdgeDetailWidgetActions : uint8
{
	None,
	Swap,
	Edit,
	Delete
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
	class UModumateButtonUserWidget* ButtonSwap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonEdit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDelete;

	int32 CurrentValue;

	UPROPERTY()
	FOnInstPropEdgeDetailButtonPress ButtonPressedEvent;

	void RegisterValue(UObject* Source, int32 DetailID);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnClickedSwap();

	UFUNCTION()
	void OnClickedEdit();

	UFUNCTION()
	void OnClickedDelete();

	EEdgeDetailWidgetActions LastClickedAction = EEdgeDetailWidgetActions::None;
};
