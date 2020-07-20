// Copyright 2020 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/ModumateComboBoxString.h"
#include "Blueprint/UserWidget.h"
#include "UI/Custom/ModumateComboBoxStringItem.h"

UModumateComboBoxString::UModumateComboBoxString(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OnGenerateWidgetEvent.BindDynamic(this, &UModumateComboBoxString::OnComboBoxGenerateWidget);
}

UWidget* UModumateComboBoxString::OnComboBoxGenerateWidget(FString SelectedItem)
{
	UModumateComboBoxStringItem* newItem = CreateWidget<UModumateComboBoxStringItem>(this, ItemWidgetClass);
	newItem->BuildItem(FText::FromString(SelectedItem));
	return newItem;
}
