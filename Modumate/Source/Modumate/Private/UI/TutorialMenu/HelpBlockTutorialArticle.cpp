// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/HelpBlockTutorialArticle.h"

#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/TutorialMenu/HelpMenu.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/SizeBox.h"
#include "WebBrowserWidget/Public/WebBrowser.h"

UHelpBlockTutorialArticle::UHelpBlockTutorialArticle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHelpBlockTutorialArticle::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonGoBack && WebBrowserVideo))
	{
		return false;
	}

	ButtonGoBack->ModumateButton->OnReleased.AddDynamic(this, &UHelpBlockTutorialArticle::OnButtonGoBackPress);

	return true;
}

void UHelpBlockTutorialArticle::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();

}

void UHelpBlockTutorialArticle::BuildTutorialArticle(const FGuid& InTutorialGUID)
{
	CurrentTutorialGUID = InTutorialGUID;
	auto tutorialNode = EMPlayerController->EditModelUserWidget->HelpMenuBP->AllTutorialNodesByGUID.Find(CurrentTutorialGUID);
	if (tutorialNode)
	{
		ArticleSubtitle->ChangeText(FText::FromString(tutorialNode->Title));
		ArticleMainText->ChangeText(FText::FromString(tutorialNode->Body));

		// Do not show video if VideoURL is empty
		SizeBoxVideo->SetVisibility(tutorialNode->VideoURL.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

		if (ensureAlways(WebBrowserVideo) &&!tutorialNode->VideoURL.IsEmpty())
		{
			ToggleWebBrowser(true);
			WebBrowserVideo->LoadURL(tutorialNode->VideoURL);
		}
	}
}

void UHelpBlockTutorialArticle::ToggleWebBrowser(bool bEnable)
{
	if (bEnable)
	{
		SizeBoxVideo->AddChild(WebBrowserVideo);
	}
	else
	{
		// Stop the browser completely by removing it
		// TODO: Investigating is this safe to do?
		WebBrowserVideo->RemoveFromParent();
	}
}

void UHelpBlockTutorialArticle::OnButtonGoBackPress()
{
	EMPlayerController->EditModelUserWidget->HelpMenuBP->ResetHelpWebBrowser();
	EMPlayerController->EditModelUserWidget->HelpMenuBP->ToLibraryMenu();
}
