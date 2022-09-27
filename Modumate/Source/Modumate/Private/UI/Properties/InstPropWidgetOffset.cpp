// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetOffset.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Custom/ModumateComboBoxString.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateDocument.h"


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

		EDimensionOffsetType displayOffsetType = CurrentValue.type;
		auto offsetTypeEnum = StaticEnum<EDimensionOffsetType>();
		int32 offsetTypeIdx = offsetTypeEnum->GetIndexByValue(static_cast<int64>(CurrentValue.type));
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

	if (CurrentValue.type == EDimensionOffsetType::Custom)
	{
		const auto& settings = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController())->GetDocument()->GetCurrentSettings();
		CustomValueTitle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CustomValueText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CustomValueText->ModumateEditableTextBox->SetText(UModumateDimensionStatics::CentimetersToDisplayText(CurrentValue.customValue,1,settings.DimensionType,settings.DimensionUnit));
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
	if (CurrentValue.type != selectedType)
	{
		CurrentValue.type = selectedType;
		PostValueChanged();
	}
}

void UInstPropWidgetOffset::OnCustomValueTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod)
{
	if ((CurrentValue.type == EDimensionOffsetType::Custom) &&
		((CommitMethod == ETextCommit::OnEnter) || (CommitMethod == ETextCommit::OnUserMovedFocus)))
	{
		auto enteredDimension = UModumateDimensionStatics::StringToFormattedDimension(NewText.ToString());
		if (CurrentValue.customValue != enteredDimension.Centimeters)
		{
			CurrentValue.customValue = enteredDimension.Centimeters;
			PostValueChanged();
		}
	}
}
