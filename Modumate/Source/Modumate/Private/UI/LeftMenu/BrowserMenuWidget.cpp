// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/BrowserMenuWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/LeftMenu/BrowserBlockSettings.h"
#include "UI/LeftMenu/BrowserBlockExport.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Online/ModumateAccountManager.h"

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
