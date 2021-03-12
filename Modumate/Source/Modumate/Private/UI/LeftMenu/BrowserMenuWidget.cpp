// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/BrowserMenuWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/LeftMenu/BrowserBlockSettings.h"
#include "UI/LeftMenu/BrowserBlockExport.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Online/ModumateAccountManager.h"

#define LOCTEXT_NAMESPACE "ModumateBrowserMenu"

UBrowserMenuWidget::UBrowserMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBrowserMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBrowserMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
	BrowserBlockSettingsWidget->ParentBrowserMenuWidget = this;
	BrowserBlockExportWidget->ParentBrowserMenuWidget = this;
}

void UBrowserMenuWidget::BuildBrowserMenu()
{
	NCPNavigatorWidget->BuildNCPNavigator();

	ToggleExportWidgetMenu(false);
}

void UBrowserMenuWidget::ToggleExportWidgetMenu(bool bEnable)
{
	if (bEnable)
	{
		auto* gameInstance = EMPlayerController->GetGameInstance<UModumateGameInstance>();
		auto browserBlockExportWeak = MakeWeakObjectPtr<UBrowserBlockExport>(BrowserBlockExportWidget);

		BrowserBlockExportWidget->ExportRemainText->ChangeText(LOCTEXT("CalculatingRemainingExports", "Calculating remaining exports."));

		if (!gameInstance->GetAccountManager()->RequestServiceRemaining(FModumateAccountManager::ServiceQuantityEstimates,
			[browserBlockExportWeak](FString ServiceName, bool bSuccess, bool bLimited, int32 Remaining)
		{
			static const FText remainingString = LOCTEXT("RemainingExports", "This will use 1 of {0} remaining exports.");
			UBrowserBlockExport* BrowserBlockExportWidget = browserBlockExportWeak.Get();
			if (!BrowserBlockExportWidget)
			{
				return;
			}
			if (bSuccess)
			{
				BrowserBlockExportWidget->ExportRemainText->ChangeText(bLimited ? FText::Format(remainingString, Remaining)
					: FText());
				BrowserBlockExportWidget->ButtonExport->SetIsEnabled(!bLimited || Remaining != 0);
			}
			else
			{
				BrowserBlockExportWidget->ButtonExport->SetIsEnabled(false);
			}
		}))
		{
			BrowserBlockExportWidget->ButtonExport->SetIsEnabled(false);
		}
	}
	BrowserBlockSettingsWidget->SetVisibility(bEnable ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	BrowserBlockExportWidget->SetVisibility(bEnable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (EMPlayerController)
	{
		BrowserBlockSettingsWidget->ToggleEnableExportButton(true);
	}
}

void UBrowserMenuWidget::AskToExportEstimates()
{
	if (EMPlayerController)
	{
		EMPlayerController->OnCreateQuantitiesCsv();
	}
}

#undef LOCTEXT_NAMESPACE