// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/AlertAccountDialogWidget.h"

#include "Online/ModumateCloudConnection.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"


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

	if (!(AlertTextBlock && ButtonInfoLink && ButtonConfirm && ButtonDismiss))
	{
		return false;
	}

	ButtonInfoLink->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonInfoLink);
	ButtonConfirm->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonConfirm);
	ButtonDismiss->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonDismiss);

	return true;
}

void UAlertAccountDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UAlertAccountDialogWidget::ShowDialog(const FText& AlertText, const FText& ConfirmText, const TFunction<void()>& InConfirmCallback, bool bShowLinkButton)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	ButtonInfoLink->SetVisibility(bShowLinkButton ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	bool bShowConfirm = !ConfirmText.IsEmpty();
	ButtonConfirm->GetParent()->SetVisibility(bShowConfirm ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonConfirm->ButtonText->SetText(ConfirmText);
	AlertTextBlock->ModumateTextBlock->SetText(AlertText);
	ConfirmCallback = InConfirmCallback;
}

void UAlertAccountDialogWidget::OnReleaseButtonInfoLink()
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto gameInstance = controller ? controller->GetGameInstance<UModumateGameInstance>() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (cloudConnection.IsValid())
	{
		FString fullAccountURL = cloudConnection->GetCloudRootURL() / ButtonInfoLinkURL;
		FPlatformProcess::LaunchURL(*fullAccountURL, nullptr, nullptr);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UAlertAccountDialogWidget::OnReleaseButtonConfirm()
{
	SetVisibility(ESlateVisibility::Collapsed);
	OnPressedConfirm.Broadcast();
	if (ConfirmCallback)
	{
		ConfirmCallback();
	}
}

void UAlertAccountDialogWidget::OnReleaseButtonDismiss()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
