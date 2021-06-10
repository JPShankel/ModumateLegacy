// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetCombo.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Custom/ModumateComboBoxString.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"

bool UInstPropWidgetCombo::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	CurrentValue = 0;

	ComboBox->OnSelectionChanged.AddDynamic(this, &UInstPropWidgetCombo::OnSelectionChanged);

	return true;
}

void UInstPropWidgetCombo::ResetInstProp()
{
	Super::ResetInstProp();

	ValueChangedEvent.Clear();
	CurrentValue = 0;
}

void UInstPropWidgetCombo::RegisterValue(UObject* Source, const int32 SelectedValue)
{
	OnRegisteredValue(Source, CurrentValue == SelectedValue);
	CurrentValue = SelectedValue;
}

void UInstPropWidgetCombo::DisplayValue()
{
	ComboBox->SetSelectedIndex(CurrentValue);
}

void UInstPropWidgetCombo::BroadcastValueChanged()
{
	ValueChangedEvent.Broadcast(CurrentValue);
}

void UInstPropWidgetCombo::OnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	int32 newIndex = ComboBox->GetSelectedIndex();
	if (newIndex >= 0 && CurrentValue != newIndex)
	{
		CurrentValue = newIndex;
		PostValueChanged();
	}
}
