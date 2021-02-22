// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetScale.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropScaleSet, float, NewValue);

UCLASS()
class MODUMATE_API UInstPropWidgetScale : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* ScaleText;

	UPROPERTY()
	float CurrentValue;

	UPROPERTY()
	FOnInstPropScaleSet ValueChangedEvent;

	UFUNCTION()
	void RegisterValue(UObject* Source, float ScaleValue);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnScaleTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod);
};
