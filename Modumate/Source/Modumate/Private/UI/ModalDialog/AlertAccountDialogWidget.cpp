// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/AlertAccountDialogWidget.h"

#include "Online/ModumateCloudConnection.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuGameMode.h"


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

	if (!(TitleTextBlock && AlertTextBlock && ButtonConfirm && ButtonDismiss))
	{
		return false;
	}

	ButtonConfirm->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonConfirm);
	ButtonDismiss->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonDismiss);

	if (ButtonInfoLink)
	{
		ButtonInfoLink->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonInfoLink);
	}

	if (ButtonOfflineProject)
	{
		ButtonOfflineProject->ModumateButton->OnReleased.AddDynamic(this, &UAlertAccountDialogWidget::OnReleaseButtonOfflineProject);
	}

	return true;
}

void UAlertAccountDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UAlertAccountDialogWidget::ShowDialog(const FText& TitleText, const FText& AlertText, const FText& ConfirmText, bool bShowUpgrade, bool bShowDismiss, const TFunction<void()>& InConfirmCallback, const TFunction<void()>& InDismissCallback)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	bool bShowConfirm = !ConfirmText.IsEmpty();
	ButtonConfirm->GetParent()->SetVisibility(bShowConfirm ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonConfirm->ButtonText->SetText(ConfirmText);

	ButtonDismiss->SetVisibility(bShowDismiss ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (ButtonInfoLink)
	{
		ButtonInfoLink->SetVisibility(bShowUpgrade ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	TitleTextBlock->ModumateTextBlock->SetText(TitleText);
	AlertTextBlock->ModumateTextBlock->SetText(AlertText);
	ConfirmCallback = InConfirmCallback;
	DismissCallback = InDismissCallback;
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
	if (DismissCallback)
	{
		DismissCallback();
	}
}

void UAlertAccountDialogWidget::OnReleaseButtonOfflineProject()
{
	UWorld* world = GetWorld();
	AMainMenuGameMode* mainMenuGameMode = world ? world->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
	if (mainMenuGameMode)
	{
		mainMenuGameMode->OpenPendingOfflineProject();
	}
}
