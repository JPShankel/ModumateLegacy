// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/HelpBlockTutorialSearch.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/TutorialMenu/HelpMenu.h"
#include "UI/LeftMenu/NCPNavigator.h"

UHelpBlockTutorialSearch::UHelpBlockTutorialSearch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHelpBlockTutorialSearch::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (ComponentSearchBar)
	{
		ComponentSearchBar->ModumateEditableTextBox->OnTextChanged.AddDynamic(this, &UHelpBlockTutorialSearch::OnSearchBarChanged);
	}

	return true;
}

void UHelpBlockTutorialSearch::NativeConstruct()
{
	Super::NativeConstruct();
}

void UHelpBlockTutorialSearch::OnSearchBarChanged(const FText& NewText)
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller)
	{
		controller->EditModelUserWidget->HelpMenuBP->NCPNavigator->BuildNCPNavigator(EPresetCardType::TutorialCategory);
	}
}
