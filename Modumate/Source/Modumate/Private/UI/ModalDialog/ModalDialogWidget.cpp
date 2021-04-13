// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/ModalDialogWidget.h"
#include "UI/ModalDialog/AlertAccountDialogWidget.h"


UModalDialogWidget::UModalDialogWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModalDialogWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!(AlertAccountDialogWidgetBP && DeletePresetDialogWidgetBP))
	{
		return false;
	}
	return true;
}

void UModalDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModalDialogWidget::ShowAlertAccountDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback)
{
	HideAllWidgets();
	AlertAccountDialogWidgetBP->SetVisibility(ESlateVisibility::Visible);
	AlertAccountDialogWidgetBP->ShowDialog(AlertText, ConfirmText, InConfirmCallback);
}

void UModalDialogWidget::ShowDeletePresetDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback)
{
	HideAllWidgets();
	DeletePresetDialogWidgetBP->SetVisibility(ESlateVisibility::Visible);
	DeletePresetDialogWidgetBP->ShowDialog(AlertText, ConfirmText, InConfirmCallback);
}

void UModalDialogWidget::HideAllWidgets()
{
	AlertAccountDialogWidgetBP->SetVisibility(ESlateVisibility::Collapsed);
	DeletePresetDialogWidgetBP->SetVisibility(ESlateVisibility::Collapsed);
}
