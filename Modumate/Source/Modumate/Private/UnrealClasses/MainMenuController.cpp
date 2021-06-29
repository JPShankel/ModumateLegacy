// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/MainMenuController.h"

#include "UI/StartMenu/StartRootMenuWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"

void AMainMenuController::BeginPlay()
{
	Super::BeginPlay();

	StartRootMenuWidget = CreateWidget<UStartRootMenuWidget>(this, StartRootMenuWidgetClass);
	if (StartRootMenuWidget)
	{
		StartRootMenuWidget->AddToViewport();
		bShowMouseCursor = true;

		// If the user is starting the main menu because of some kind of error status (like failing to join a multiplayer session), then display it now.
		FText statusMessage;
		auto* gameInstance = GetGameInstance<UModumateGameInstance>();
		if (gameInstance && gameInstance->CheckMainMenuStatus(statusMessage))
		{
			StartRootMenuWidget->ShowModalStatus(statusMessage, true);
		}
	}
}
