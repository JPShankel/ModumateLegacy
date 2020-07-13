// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashBlockHomeWidget.h"

USplashBlockHomeWidget::USplashBlockHomeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USplashBlockHomeWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void USplashBlockHomeWidget::NativeConstruct()
{
	Super::NativeConstruct();
}