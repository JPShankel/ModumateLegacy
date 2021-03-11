// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/BrowserBlockSettings.h"
#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/LeftMenu/BrowserMenuWidget.h"


UBrowserBlockSettings::UBrowserBlockSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBrowserBlockSettings::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ButtonIconWithTextExportWidget)
	{
		return false;
	}

	ButtonIconWithTextExportWidget->ModumateButton->OnReleased.AddDynamic(this, &UBrowserBlockSettings::OnExportButtonReleased);

	return true;
}

void UBrowserBlockSettings::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBrowserBlockSettings::OnExportButtonReleased()
{
	if (ensureAlways(ParentBrowserMenuWidget))
	{
		ParentBrowserMenuWidget->ToggleExportWidgetMenu(true);
	}
}

void UBrowserBlockSettings::ToggleEnableExportButton(bool bEnable)
{
	ButtonIconWithTextExportWidget->SetIsEnabled(bEnable);
}
