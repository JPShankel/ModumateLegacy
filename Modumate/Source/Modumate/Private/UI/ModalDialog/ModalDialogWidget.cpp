// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/ModalDialogWidget.h"
#include "UI/ModalDialog/AlertAccountDialogWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "Components/SizeBox.h"
#include "Components/BackgroundBlur.h"
#include "Components/RichTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"


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
	if (!AlertAccountDialogWidgetBP)
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

void UModalDialogWidget::ShowUploadOfflineProjectDialog(const FText& TitleText, const FText& StatusText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback)
{
	HideAllWidgets();
	UploadOfflineProjectDialogWidgetBP->SetVisibility(ESlateVisibility::Visible);
	UploadOfflineProjectDialogWidgetBP->ShowDialog(TitleText, StatusText, ConfirmText, false, true, InConfirmCallback);
}

void UModalDialogWidget::ShowUploadOfflineDoneDialog(const FText& TitleText, const FText& StatusText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback)
{
	HideAllWidgets();
	UploadOfflineDoneDialogWidgetBP->SetVisibility(ESlateVisibility::Visible);
	UploadOfflineDoneDialogWidgetBP->ShowDialog(TitleText, StatusText, ConfirmText, false, true, InConfirmCallback);
}

void UModalDialogWidget::HideAllWidgets()
{
	AlertAccountDialogWidgetBP->SetVisibility(ESlateVisibility::Collapsed);
	UploadOfflineProjectDialogWidgetBP->SetVisibility(ESlateVisibility::Collapsed);
	UploadOfflineDoneDialogWidgetBP->SetVisibility(ESlateVisibility::Collapsed);

	SizeBox_Dialog->SetVisibility(ESlateVisibility::Collapsed);
	InputBlocker->SetVisibility(ESlateVisibility::Collapsed);
}

void UModalDialogWidget::CreateModalDialog(const FText& HeaderText, const FText& BodyText, const TArray<FModalButtonParam>& ModalButtonParam)
{
	// TODO Block or toggle input
	HideAllWidgets();
	HorizontalBox_Buttons->ClearChildren();

	SizeBox_Dialog->SetVisibility(ESlateVisibility::Visible);
	InputBlocker->SetVisibility(ESlateVisibility::Visible);

	RichTextBlock_Header->SetText(HeaderText);
	RichTextBlock_Body->SetText(BodyText);

	for (const auto curParam : ModalButtonParam)
	{
		TSubclassOf<class UModumateButtonUserWidget> buttonClass = ButtonClassSelector(curParam.ModalButtonStyle);

		UModumateButtonUserWidget* newButton = CreateWidget<UModumateButtonUserWidget>(this, buttonClass);
		if (newButton)
		{
			newButton->BuildFromCallBack(curParam.ButtonText, curParam.CallbackTask);
			HorizontalBox_Buttons->AddChildToHorizontalBox(newButton);
			UHorizontalBoxSlot* horizontalBoxSlot = Cast<UHorizontalBoxSlot>(newButton->Slot);
			if (horizontalBoxSlot)
			{
				horizontalBoxSlot->SetPadding(curParam.ButtonMargin);
			}
		}
	}
}

TSubclassOf<class UModumateButtonUserWidget> UModalDialogWidget::ButtonClassSelector(EModalButtonStyle Style) const
{
	switch (Style)
	{
	case EModalButtonStyle::Green:
		return GreenButtonClass;
	case EModalButtonStyle::Red:
		return RedButtonClass;
	}
	return DefaultButtonClass;
}

#undef LOCTEXT_NAMESPACE
