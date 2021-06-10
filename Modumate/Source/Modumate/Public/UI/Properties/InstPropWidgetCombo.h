// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/DimensionOffset.h"
#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetCombo.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropComboSet, int32, NewValue);

class UComboBoxString;

UCLASS()
class MODUMATE_API UInstPropWidgetCombo : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UComboBoxString* ComboBox;

	UPROPERTY()
	int32 CurrentValue;

	UPROPERTY()
	FOnInstPropComboSet ValueChangedEvent;

	UFUNCTION()
	void RegisterValue(UObject* Source, const int32 SelectedValue);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
};
