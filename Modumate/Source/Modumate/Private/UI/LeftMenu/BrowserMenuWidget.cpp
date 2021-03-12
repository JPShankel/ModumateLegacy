// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/BrowserMenuWidget.h"

#include "Components/RichTextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/EditModelUserWidget.h"
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

		BrowserBlockExportWidget->LimitedExportBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BrowserBlockExportWidget->UpgradeText->SetText(EMPlayerController->EditModelUserWidget->GetPlanUpgradeRichText());
		BrowserBlockExportWidget->UpgradeText->SetVisibility(ESlateVisibility::Collapsed);
		BrowserBlockExportWidget->ButtonExport->SetIsEnabled(false);

		if (gameInstance->GetAccountManager()->RequestServiceRemaining(FModumateAccountManager::ServiceQuantityEstimates,
			[browserBlockExportWeak](FString ServiceName, bool bSuccess, bool bLimited, int32 Remaining)
			{
				UBrowserBlockExport* BrowserBlockExportWidget = browserBlockExportWeak.Get();
				if (!BrowserBlockExportWidget)
				{
					return;
				}

				if (bSuccess)
				{
					FText remainingFormat = (Remaining > 0) ?
						LOCTEXT("ExportsRemainingSome", "This will use 1 of {0} remaining exports.") :
						LOCTEXT("ExportsRemainingNone", "You have no exports remaining.");

					BrowserBlockExportWidget->LimitedExportBox->SetVisibility(bLimited ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
					BrowserBlockExportWidget->ExportRemainText->ChangeText(bLimited ? FText::Format(remainingFormat, Remaining) : FText::GetEmpty());
					BrowserBlockExportWidget->UpgradeText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					BrowserBlockExportWidget->ButtonExport->SetIsEnabled(!bLimited || (Remaining > 0));
				}
				else
				{
					BrowserBlockExportWidget->LimitedExportBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					BrowserBlockExportWidget->ExportRemainText->ChangeText(LOCTEXT("ExportsError", "Exports are unavailable."));
					BrowserBlockExportWidget->UpgradeText->SetVisibility(ESlateVisibility::Collapsed);
					BrowserBlockExportWidget->ButtonExport->SetIsEnabled(false);
				}
			}))
		{
			BrowserBlockExportWidget->ExportRemainText->ChangeText(LOCTEXT("CalculatingRemainingExports", "Calculating remaining exports."));
		}
		else
		{
			BrowserBlockExportWidget->ExportRemainText->ChangeText(LOCTEXT("ExportsError", "Exports are unavailable."));
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
		// Create the quantity export .csv, and upon successful notification of the server about the export usage,
		// update the export menu to reflect the new number of remaining exports.
		auto weakThis = MakeWeakObjectPtr<UBrowserMenuWidget>(this);
		EMPlayerController->OnCreateQuantitiesCsv([weakThis](FString serviceName, bool bUsageNotificationSuccess) {
			if (bUsageNotificationSuccess && weakThis.IsValid())
			{
				weakThis->ToggleExportWidgetMenu(true);
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE
