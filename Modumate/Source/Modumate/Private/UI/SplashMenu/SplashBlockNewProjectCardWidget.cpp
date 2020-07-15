// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashBlockNewProjectCardWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Kismet/GameplayStatics.h"

USplashBlockNewProjectCardWidget::USplashBlockNewProjectCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USplashBlockNewProjectCardWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ModumateButtonNewProject)
	{
		return false;
	}
	ModumateButtonNewProject->OnReleased.AddDynamic(this, &USplashBlockNewProjectCardWidget::OnButtonReleasedNewProject);

	return true;
}

void USplashBlockNewProjectCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USplashBlockNewProjectCardWidget::OnButtonReleasedNewProject()
{
	UGameplayStatics::OpenLevel(this, NewLevelName, true);
}