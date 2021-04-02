// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetLinearDimension.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropDimensionSet, float, NewValue);

UCLASS()
class MODUMATE_API UInstPropWidgetLinearDimension : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* DimensionText;

	UPROPERTY()
	float CurrentValue;

	UPROPERTY()
	FOnInstPropDimensionSet ValueChangedEvent;

	UFUNCTION()
	void RegisterValue(UObject* Source, float DimensionValue);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnDimensionTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod);
};
