// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/ModalDialogWidget.h"
#include "UI/ModalDialog/AlertAccountDialogWidget.h"

#define LOCTEXT_NAMESPACE "ModumateModalDialog"

const FText UModalDialogWidget::DefaultTitleText(LOCTEXT("DefaultTitle", "ALERT"));

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

void UModalDialogWidget::ShowStatusDialog(const FText& TitleText, const FText& StatusText, bool bAllowDismiss)
{
	HideAllWidgets();
	AlertAccountDialogWidgetBP->SetVisibility(ESlateVisibility::Visible);
	AlertAccountDialogWidgetBP->ShowDialog(TitleText, StatusText, FText::GetEmpty(), false, bAllowDismiss, nullptr);
}

void UModalDialogWidget::ShowAlertAccountDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback)
{
	HideAllWidgets();
	AlertAccountDialogWidgetBP->SetVisibility(ESlateVisibility::Visible);
	AlertAccountDialogWidgetBP->ShowDialog(DefaultTitleText, AlertText, ConfirmText, true, true, InConfirmCallback);
}

void UModalDialogWidget::ShowDeletePresetDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback)
{
	HideAllWidgets();
	DeletePresetDialogWidgetBP->SetVisibility(ESlateVisibility::Visible);
	DeletePresetDialogWidgetBP->ShowDialog(DefaultTitleText, AlertText, ConfirmText, false, true, InConfirmCallback);
}

void UModalDialogWidget::HideAllWidgets()
{
	AlertAccountDialogWidgetBP->SetVisibility(ESlateVisibility::Collapsed);
	DeletePresetDialogWidgetBP->SetVisibility(ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
