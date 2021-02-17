// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/DimensionOffset.h"
#include "UI/Properties/InstPropWidgetBase.h"

#include "InstPropWidgetOffset.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstPropOffsetSet, const FDimensionOffset&, NewValue);

UCLASS()
class MODUMATE_API UInstPropWidgetOffset : public UInstPropWidgetBase
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void ResetInstProp() override;
	virtual void DisplayValue() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget* OffsetTypeDropdown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* CustomValueTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* CustomValueText;

	UPROPERTY()
	FDimensionOffset CurrentValue;

	UPROPERTY()
	FOnInstPropOffsetSet ValueChangedEvent;

	UFUNCTION()
	void RegisterValue(UObject* Source, const FDimensionOffset& OffsetValue);

protected:
	virtual void BroadcastValueChanged() override;

	UFUNCTION()
	void OnOffsetTypeSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnCustomValueTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod);

	TMap<FString, EDimensionOffsetType> DimensionTypeByString;
};
