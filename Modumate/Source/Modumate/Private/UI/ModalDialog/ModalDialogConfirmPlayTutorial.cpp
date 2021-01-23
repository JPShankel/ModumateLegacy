// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ModalDialog/ModalDialogConfirmPlayTutorial.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "UnrealClasses/EditModelPlayerController.h"



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
	if (controller && controller->CheckSaveModel())
	{
		FPlatformProcess::LaunchURL(*CurrentVideoLink, nullptr, nullptr);
		controller->LoadModelFilePath(CurrentProjectFilePath, true, false, false);
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
