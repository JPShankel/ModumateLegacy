// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/AlertAccountDialogWidget.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"



UAlertAccountDialogWidget::UAlertAccountDialogWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UAlertAccountDialogWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonInfoLink && ButtonDismiss))
	{
		return false;
	}

	ButtonInfoLink->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonInfoLink);
	ButtonDismiss->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonDismiss);
	return true;
}

void UAlertAccountDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UAlertAccountDialogWidget::OnReleaseButtonInfoLink()
{
	FPlatformProcess::LaunchURL(*ButtonInfoLinkURL, nullptr, nullptr);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UAlertAccountDialogWidget::OnReleaseButtonDismiss()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
