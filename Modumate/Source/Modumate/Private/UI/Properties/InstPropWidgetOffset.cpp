// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetOffset.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Custom/ModumateComboBoxString.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"


bool UInstPropWidgetOffset::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(OffsetTypeDropdown && CustomValueTitle && CustomValueText))
	{
		return false;
	}

	OffsetTypeDropdown->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UInstPropWidgetOffset::OnOffsetTypeSelected);
	CustomValueText->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UInstPropWidgetOffset::OnCustomValueTextCommitted);

	DimensionTypeByString.Reset();
	OffsetTypeDropdown->ComboBoxStringJustification->ClearOptions();
	auto offsetTypeEnum = StaticEnum<EDimensionOffsetType>();
	int32 numEnums = offsetTypeEnum->NumEnums();
	if (offsetTypeEnum->ContainsExistingMax())
	{
		numEnums--;
	}

	for (int32 offsetTypeIdx = 0; offsetTypeIdx < numEnums; ++offsetTypeIdx)
	{
		EDimensionOffsetType offsetTypeValue = static_cast<EDimensionOffsetType>(offsetTypeEnum->GetValueByIndex(offsetTypeIdx));
		FText offsetTypeDisplayName = offsetTypeEnum->GetDisplayNameTextByIndex(offsetTypeIdx);
		FString offsetTypeDisplayString = offsetTypeDisplayName.ToString();

		OffsetTypeDropdown->ComboBoxStringJustification->AddOption(offsetTypeDisplayString);
		DimensionTypeByString.Add(offsetTypeDisplayString, offsetTypeValue);
	}

	return true;
}

void UInstPropWidgetOffset::ResetInstProp()
{
	Super::ResetInstProp();

	ValueChangedEvent.Clear();
	CurrentValue = FDimensionOffset();
}

void UInstPropWidgetOffset::DisplayValue()
{
	const FText& mixedText = UInstPropWidgetBase::GetMixedDisplayText();
	const FString& mixedString = mixedText.ToString();

	if (bConsistentValue)
	{
		OffsetTypeDropdown->ComboBoxStringJustification->RemoveOption(mixedString);

		EDimensionOffsetType displayOffsetType = CurrentValue.Type;
		auto offsetTypeEnum = StaticEnum<EDimensionOffsetType>();
		int32 offsetTypeIdx = offsetTypeEnum->GetIndexByValue(static_cast<int64>(CurrentValue.Type));
		if (offsetTypeIdx != INDEX_NONE)
		{
			OffsetTypeDropdown->ComboBoxStringJustification->SetSelectedIndex(offsetTypeIdx);
		}
	}
	else
	{
		OffsetTypeDropdown->ComboBoxStringJustification->AddOption(mixedString);
		OffsetTypeDropdown->ComboBoxStringJustification->SetSelectedOption(mixedString);
	}

	if (CurrentValue.Type == EDimensionOffsetType::Custom)
	{
		CustomValueTitle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CustomValueText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CustomValueText->ModumateEditableTextBox->SetText(UModumateDimensionStatics::CentimetersToDisplayText(CurrentValue.CustomValue));
	}
	else
	{
		CustomValueTitle->SetVisibility(ESlateVisibility::Collapsed);
		CustomValueText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInstPropWidgetOffset::RegisterValue(UObject* Source, const FDimensionOffset& OffsetValue)
{
	OnRegisteredValue(Source, CurrentValue == OffsetValue);
	CurrentValue = OffsetValue;
}

void UInstPropWidgetOffset::BroadcastValueChanged()
{
	ValueChangedEvent.Broadcast(CurrentValue);
}

void UInstPropWidgetOffset::OnOffsetTypeSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!DimensionTypeByString.Contains(SelectedItem))
	{
		return;
	}

	EDimensionOffsetType selectedType = DimensionTypeByString[SelectedItem];
	if (CurrentValue.Type != selectedType)
	{
		CurrentValue.Type = selectedType;
		PostValueChanged();
	}
}

void UInstPropWidgetOffset::OnCustomValueTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod)
{
	if ((CurrentValue.Type == EDimensionOffsetType::Custom) &&
		((CommitMethod == ETextCommit::OnEnter) || (CommitMethod == ETextCommit::OnUserMovedFocus)))
	{
		auto enteredDimension = UModumateDimensionStatics::StringToFormattedDimension(NewText.ToString());
		if (CurrentValue.CustomValue != enteredDimension.Centimeters)
		{
			CurrentValue.CustomValue = enteredDimension.Centimeters;
			PostValueChanged();
		}
	}
}
