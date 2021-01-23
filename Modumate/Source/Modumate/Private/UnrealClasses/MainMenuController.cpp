// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/MainMenuController.h"
#include "UI/StartMenu/StartRootMenuWidget.h"

void AMainMenuController::BeginPlay()
{
	Super::BeginPlay();

	StartRootMenuWidget = CreateWidget<UStartRootMenuWidget>(this, StartRootMenuWidgetClass);
	if (StartRootMenuWidget)
	{
		StartRootMenuWidget->AddToViewport();
		bShowMouseCursor = true;
	}
}
