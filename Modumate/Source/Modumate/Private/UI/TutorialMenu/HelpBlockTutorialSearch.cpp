// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/HelpBlockTutorialSearch.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/TutorialMenu/HelpMenu.h"
#include "Online/ModumateAnalyticsStatics.h"


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

void UHelpBlockTutorialSearch::SendAnalytics()
{
	if (!AnalyticsString.IsEmpty())
	{
		UModumateAnalyticsStatics::RecordEventCustomString(this, EModumateAnalyticsCategory::Tutorials, UHelpMenu::AnalyticsSearchEvent, AnalyticsString);
		AnalyticsString.Empty();
	}
}

void UHelpBlockTutorialSearch::OnSearchBarChanged(const FText& NewText)
{
	if (!GetWorld() || !GetWorld()->GetFirstPlayerController())
	{
		return;
	}

	auto& timerManager = GetWorld()->GetFirstPlayerController()->GetWorldTimerManager();

	timerManager.ClearTimer(AnalyticsTimer);
	timerManager.SetTimer(AnalyticsTimer, this, &UHelpBlockTutorialSearch::SendAnalytics, 1.0f, false, 3.0f);
	AnalyticsString = NewText.ToString();

	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller)
	{
		controller->EditModelUserWidget->HelpMenuBP->ToLibraryMenu();
	}
}
