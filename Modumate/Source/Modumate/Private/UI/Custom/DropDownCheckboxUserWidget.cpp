// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/DropDownCheckboxUserWidget.h"
#include "UI/Custom/ModumateComboBoxString.h"
#include "UnrealClasses/TooltipManager.h"

UDropDownCheckboxUserWidget::UDropDownCheckboxUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDropDownCheckboxUserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ModumateComboBox)
	{
		return false;
	}

	ModumateComboBox->OnSelectionChanged.AddDynamic(this, &UDropDownCheckboxUserWidget::OnDropDownSelectionChanged);
	ToolTipWidgetDelegate.BindDynamic(this, &UDropDownCheckboxUserWidget::OnTooltipWidget);
	return true;
}

void UDropDownCheckboxUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UDropDownCheckboxUserWidget::OnDropDownSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// DropDownCheckbox doesn't use selection. It allows user to toggle check boxes in its item list 
	ModumateComboBox->ClearSelection();
}

UWidget* UDropDownCheckboxUserWidget::OnTooltipWidget()
{
	return UTooltipManager::GenerateTooltipNonInputWidget(TooltipID, this);
}
