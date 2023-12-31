// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/ModalDialogWidget.h"
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

	return true;
}

void UModalDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModalDialogWidget::HideAllWidgets()
{
	SizeBox_Dialog->SetVisibility(ESlateVisibility::Collapsed);
	InputBlocker->SetVisibility(ESlateVisibility::Collapsed);
}

void UModalDialogWidget::CreateModalDialog(const FText& HeaderText, const FText& BodyText, const TArray<FModalButtonParam>& ModalButtonParam)
{
	EnableInputHandler(false);
	HideAllWidgets();
	HorizontalBox_Buttons->ClearChildren();

	SizeBox_Dialog->SetVisibility(ESlateVisibility::Visible);
	InputBlocker->SetVisibility(ESlateVisibility::Visible);

	RichTextBlock_Header->SetText(HeaderText);
	RichTextBlock_Body->SetText(BodyText);

	for (const auto& curParam : ModalButtonParam)
	{
		TSubclassOf<class UModumateButtonUserWidget> buttonClass = ButtonClassSelector(curParam.ModalButtonStyle);

		UModumateButtonUserWidget* newButton = CreateWidget<UModumateButtonUserWidget>(this, buttonClass);
		if (newButton)
		{
			newButton->BuildFromModalDialogCallBack(curParam.ButtonText, curParam.CallbackTask);
			HorizontalBox_Buttons->AddChildToHorizontalBox(newButton);
			UHorizontalBoxSlot* horizontalBoxSlot = Cast<UHorizontalBoxSlot>(newButton->Slot);
			if (horizontalBoxSlot)
			{
				horizontalBoxSlot->SetPadding(curParam.ButtonMargin);
			}
		}
	}
}

void UModalDialogWidget::CloseModalDialog()
{
	HideAllWidgets();
	EnableInputHandler(true);
}

void UModalDialogWidget::EnableInputHandler(bool bEnable)
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && controller->InputHandlerComponent)
	{
		static const FName modalDialogName = TEXT("ModalDialogWidget");
		controller->InputHandlerComponent->RequestInputDisabled(modalDialogName, !bEnable);
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
