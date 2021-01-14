// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/ModalDialogConfirmPlayTutorial.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/MainMenuGameMode_CPP.h"



UModalDialogConfirmPlayTutorial::UModalDialogConfirmPlayTutorial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModalDialogConfirmPlayTutorial::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonOpenProject && ButtonCancel))
	{
		return false;
	}

	ButtonOpenProject->ModumateButton->OnReleased.AddDynamic(this, &UModalDialogConfirmPlayTutorial::OnReleaseButtonOpenProject);
	ButtonCancel->ModumateButton->OnReleased.AddDynamic(this, &UModalDialogConfirmPlayTutorial::OnReleaseButtonCancel);
	return true;
}

void UModalDialogConfirmPlayTutorial::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModalDialogConfirmPlayTutorial::OnReleaseButtonOpenProject()
{
	SetVisibility(ESlateVisibility::Collapsed);

	// Play Video
	FPlatformProcess::LaunchURL(*CurrentVideoLink, nullptr, nullptr);

	// Open tutorial map
	FString tutorialsFolderPath = FModumateUserSettings::GetTutorialsFolderPath();
	FString tutorialPath = tutorialsFolderPath / CurrentProjectFilePath;
	if (ensureAlways(IFileManager::Get().FileExists(*tutorialPath)))
	{
		AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
		if (controller)
		{
			controller->LoadModelFilePath(tutorialPath, false);
		}
		else // If EMPlayerController not found, then use main menu
		{
			AMainMenuGameMode_CPP* mainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode_CPP>();
			if (mainMenuGameMode)
			{
				mainMenuGameMode->OpenProject(tutorialPath);
			}
		}
	}
}

void UModalDialogConfirmPlayTutorial::OnReleaseButtonCancel()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UModalDialogConfirmPlayTutorial::BuildModalDialog(const FString& InProjectFilePath, const FString& InVideoLink)
{
	SetVisibility(ESlateVisibility::Visible);
	CurrentProjectFilePath = InProjectFilePath;
	CurrentVideoLink = InVideoLink;
}
