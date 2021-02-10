// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/ModalDialogConfirmPlayTutorial.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/TutorialManager.h"



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

	// Open tutorial map
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	auto gameInstance = controller ? controller->GetGameInstance<UModumateGameInstance>() : nullptr;
	if (controller && controller->CheckSaveModel() && gameInstance && gameInstance->TutorialManager)
	{
		if (CurrentWalkthroughCategory == EModumateWalkthroughCategories::None)
		{
			gameInstance->TutorialManager->OpenVideoTutorial(CurrentProjectFilePath, CurrentVideoLink);
		}
		else
		{
			gameInstance->TutorialManager->OpenWalkthroughProject(CurrentWalkthroughCategory);
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
	CurrentWalkthroughCategory = EModumateWalkthroughCategories::None;
}

void UModalDialogConfirmPlayTutorial::BuildModalDialogWalkthrough(EModumateWalkthroughCategories InWalkthroughCategory)
{
	SetVisibility(ESlateVisibility::Visible);
	CurrentWalkthroughCategory = InWalkthroughCategory;
}
