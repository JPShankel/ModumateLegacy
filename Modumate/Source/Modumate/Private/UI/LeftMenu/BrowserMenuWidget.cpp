// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/BrowserMenuWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"


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
}

void UBrowserMenuWidget::BuildBrowserMenu()
{
	NCPNavigatorWidget->BuildNCPNavigator();
}
