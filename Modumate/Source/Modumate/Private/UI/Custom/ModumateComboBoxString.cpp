// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateComboBoxString.h"
#include "Blueprint/UserWidget.h"
#include "UI/Custom/ModumateComboBoxStringItem.h"

UModumateComboBoxString::UModumateComboBoxString(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OnGenerateWidgetEvent.BindDynamic(this, &UModumateComboBoxString::OnComboBoxGenerateWidget);
}

void UModumateComboBoxString::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyCustomStyle();
}

UWidget* UModumateComboBoxString::OnComboBoxGenerateWidget(FString SelectedItem)
{
	UModumateComboBoxStringItem* newItem = CreateWidget<UModumateComboBoxStringItem>(this, ItemWidgetClass);
	if (ensureAlways(newItem))
	{
		newItem->BuildItem(FText::FromString(SelectedItem));
	}
	return newItem;
}

bool UModumateComboBoxString::ApplyCustomStyle()
{
	bool bComboBoxWidgetSuccess = false;
	bool bComboBoxItemSuccess = false;

	if (ComboBoxWidgetStyle)
	{
		const FComboBoxStyle* StylePtr = ComboBoxWidgetStyle->GetStyle<FComboBoxStyle>();
		if (StylePtr)
		{
			WidgetStyle = *StylePtr;
			bComboBoxWidgetSuccess = true;
		}
	}
	if (ComboBoxItemStyle)
	{
		const FTableRowStyle* StylePtr = ComboBoxItemStyle->GetStyle<FTableRowStyle>();
		if (StylePtr)
		{
			ItemStyle = *StylePtr;
			bComboBoxItemSuccess = true;
		}
	}

	return bComboBoxWidgetSuccess && bComboBoxItemSuccess;
}
