// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/MainMenuController.h"

#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/StartMenu/StartMenuWebBrowserWidget.h"

void AMainMenuController::BeginPlay()
{
	Super::BeginPlay();

	StartMenuWebBrowserWidget = CreateWidget<UStartMenuWebBrowserWidget>(this, StartMenuWebBrowserWidgetClass);
	if (StartMenuWebBrowserWidget)
	{
		StartMenuWebBrowserWidget->AddToViewport();
		bShowMouseCursor = true;

		// If the user is starting the main menu because of some kind of error status (like failing to join a multiplayer session), then display it now.
		FText statusMessage;
		auto* gameInstance = GetGameInstance<UModumateGameInstance>();
		if (gameInstance && gameInstance->CheckMainMenuStatus(statusMessage))
		{
			StartMenuWebBrowserWidget->ShowModalStatus(statusMessage, true);
		}
	}
}
