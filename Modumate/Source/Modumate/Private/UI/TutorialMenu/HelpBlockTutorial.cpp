// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/HelpBlockTutorial.h"
#include "UI/TutorialMenu/HelpMenu.h"
#include "UI/Custom/ModumateButtonIconTextUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/ModalDialog/ModalDialogConfirmPlayTutorial.h"


UHelpBlockTutorial::UHelpBlockTutorial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHelpBlockTutorial::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(Button_TutorialLibrary && Button_IntroWalkthrough))
	{
		return false;
	}

	Button_TutorialLibrary->ModumateButton->OnReleased.AddDynamic(this, &UHelpBlockTutorial::OnButtonReleasedTutorialLibrary);
	Button_IntroWalkthrough->ModumateButton->OnReleased.AddDynamic(this, &UHelpBlockTutorial::OnButtonReleasedIntroWalkthrough);

	return true;
}

void UHelpBlockTutorial::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHelpBlockTutorial::OnButtonReleasedTutorialLibrary()
{
	if (ensureAlways(ParentHelpMenu))
	{
		ParentHelpMenu->ToLibraryMenu();
	}
}

void UHelpBlockTutorial::OnButtonReleasedIntroWalkthrough()
{
	// TODO: Generalize modal dialog for opening walkthrough tutorial project
	if (ensureAlways(ParentHelpMenu))
	{
		ParentHelpMenu->ModalDialogConfirmPlayTutorialBP->BuildModalDialogWalkthrough(EModumateWalkthroughCategories::Beginner);
	}
}