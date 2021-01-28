// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialWalkthroughMenu.h"

#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/TutorialManager.h"
#include "UI/TutorialMenu/TutorialWalkthroughBlockIntro.h"
#include "UI/TutorialMenu/TutorialWalkthroughBlockItem.h"
#include "UI/TutorialMenu/TutorialWalkthroughBlockOutro.h"
#include "UnrealClasses/ModumateGameInstance.h"

UTutorialWalkthroughMenu::UTutorialWalkthroughMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTutorialWalkthroughMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(IntroWidget && WalkthroughItemWidget && OutroWidget))
	{
		return false;
	}

	IntroWidget->ButtonIntroProceed->ModumateButton->OnReleased.AddDynamic(this, &UTutorialWalkthroughMenu::OnReleaseButtonIntroProceed);
	IntroWidget->ButtonIntroGoBack->ModumateButton->OnReleased.AddDynamic(this, &UTutorialWalkthroughMenu::OnReleaseButtonIntroGoBack);
	WalkthroughItemWidget->ButtonWalkthroughProceed->ModumateButton->OnReleased.AddDynamic(this, &UTutorialWalkthroughMenu::OnReleaseWalkthroughButtonProceed);
	WalkthroughItemWidget->ButtonWalkthroughGoBack->ModumateButton->OnReleased.AddDynamic(this, &UTutorialWalkthroughMenu::OnReleaseButtonWalkthroughGoBack);
	OutroWidget->ButtonOutroProceed->ModumateButton->OnReleased.AddDynamic(this, &UTutorialWalkthroughMenu::OnReleaseButtonOutroProceed);
	OutroWidget->ButtonOutroGoBack->ModumateButton->OnReleased.AddDynamic(this, &UTutorialWalkthroughMenu::OnReleaseButtonOutroGoBack);

	auto world = GetWorld();
	auto gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	TutorialManager = gameInstance ? gameInstance->TutorialManager : nullptr;

	return true;
}

void UTutorialWalkthroughMenu::NativeConstruct()
{
	Super::NativeConstruct();
}

void UTutorialWalkthroughMenu::ShowWalkthroughIntro()
{
	UpdateBlockVisibility(ETutorialWalkthroughBlockStage::Intro);
}

void UTutorialWalkthroughMenu::ShowWalkthroughStep(const FText& Title, const FText& Description, const FString& VideoURL, float ProgressPCT)
{
	UpdateBlockVisibility(ETutorialWalkthroughBlockStage::WalkthroughSteps);
	WalkthroughItemWidget->TitleText->ChangeText(Title);
	WalkthroughItemWidget->DescriptionText->ChangeText(Description);

}

void UTutorialWalkthroughMenu::ShowCountdown(float CountdownSeconds)
{
	
}

void UTutorialWalkthroughMenu::ShowWalkthroughOutro()
{
	UpdateBlockVisibility(ETutorialWalkthroughBlockStage::Outro);
}

void UTutorialWalkthroughMenu::UpdateBlockVisibility(ETutorialWalkthroughBlockStage Stage)
{
	if ((Stage == ETutorialWalkthroughBlockStage::None) || !ensure(TutorialManager))
	{
		this->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	this->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	ESlateVisibility visibleIntro = Stage == ETutorialWalkthroughBlockStage::Intro ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	ESlateVisibility visibleSteps = Stage == ETutorialWalkthroughBlockStage::WalkthroughSteps ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	ESlateVisibility visibleOutro = Stage == ETutorialWalkthroughBlockStage::Outro ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	IntroWidget->SetVisibility(visibleIntro);
	WalkthroughItemWidget->SetVisibility(visibleSteps);
	OutroWidget->SetVisibility(visibleOutro);
}

void UTutorialWalkthroughMenu::OnReleaseButtonIntroProceed()
{
	TutorialManager->AdvanceWalkthrough();
}

void UTutorialWalkthroughMenu::OnReleaseButtonIntroGoBack()
{
	TutorialManager->RewindWalkthrough();
}

void UTutorialWalkthroughMenu::OnReleaseWalkthroughButtonProceed()
{
	TutorialManager->AdvanceWalkthrough();
}

void UTutorialWalkthroughMenu::OnReleaseButtonWalkthroughGoBack()
{
	TutorialManager->RewindWalkthrough();
}

void UTutorialWalkthroughMenu::OnReleaseButtonOutroProceed()
{
	TutorialManager->AdvanceWalkthrough();
}

void UTutorialWalkthroughMenu::OnReleaseButtonOutroGoBack()
{
	TutorialManager->RewindWalkthrough();
}
