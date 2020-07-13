// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashRootMenuWidget.h"

USplashRootMenuWidget::USplashRootMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USplashRootMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void USplashRootMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
}