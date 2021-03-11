// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/TutorialMenu/TutorialWalkthroughMenu.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "GameMapsSettings.h"
#include "Runtime/MediaAssets/Public/MediaPlayer.h"
#include "Runtime/MediaAssets/Public/StreamMediaSource.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
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

	MediaPlayer->OnMediaOpened.AddDynamic(this, &UTutorialWalkthroughMenu::OnUrlMediaOpened);
	MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &UTutorialWalkthroughMenu::OnUrlMediaOpenFailed);
	
	auto world = GetWorld();
	auto gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	TutorialManager = gameInstance ? gameInstance->TutorialManager : nullptr;

	return true;
}

void UTutorialWalkthroughMenu::NativeConstruct()
{
	Super::NativeConstruct();
}

void UTutorialWalkthroughMenu::ShowWalkthroughIntro(const FText& Title, const FText& Description)
{
	UpdateBlockVisibility(ETutorialWalkthroughBlockStage::Intro);
	IntroWidget->TitleText->ChangeText(Title);
	IntroWidget->DescriptionText->ChangeText(Description);
}

void UTutorialWalkthroughMenu::ShowWalkthroughStep(const FText& Title, const FText& Description, const FString& VideoURL, float ProgressPCT)
{
	UpdateBlockVisibility(ETutorialWalkthroughBlockStage::WalkthroughSteps);
	WalkthroughItemWidget->TitleText->ChangeText(Title);
	WalkthroughItemWidget->DescriptionText->ChangeText(Description);
	WalkthroughItemWidget->WalkthroughProgressBar->SetPercent(ProgressPCT);

	// Grayout next button to encourage user to try the tutorial first
	WalkthroughItemWidget->ButtonWalkthroughProceed->SwitchToDisabledStyle();
	WalkthroughItemWidget->ButtonWalkthroughProceed->ButtonText->SetText(WalkthroughNextText);

	// Clear existing timer for going to the next step in tutorial
	GetWorld()->GetTimerManager().ClearTimer(TutorialCountdownTimer);

	if (!VideoURL.IsEmpty() && MediaPlayer && StreamMediaSource)
	{
		StreamMediaSource->StreamUrl = VideoURL;
		MediaPlayer->OpenSource(StreamMediaSource);

		ToggleMediaPlayerInteraction(false);
	}

	// Do not show video if VideoURL is empty
	WalkthroughItemWidget->SizeBoxVideo->SetVisibility(VideoURL.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UTutorialWalkthroughMenu::ShowCountdown(float CountdownSeconds)
{
	WalkthroughItemWidget->ButtonWalkthroughProceed->SwitchToNormalStyle();
	TutorialCountdownStep = CountdownSeconds;
	CheckTutorialCountdownTimer();
}

void UTutorialWalkthroughMenu::ShowWalkthroughOutro(const FText& Title, const FText& Description, bool bShowProceedButton)
{
	UpdateBlockVisibility(ETutorialWalkthroughBlockStage::Outro);
	OutroWidget->TitleText->ChangeText(Title);
	OutroWidget->DescriptionText->ChangeText(Description);
	OutroWidget->ButtonOutroProceed->SetVisibility(bShowProceedButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UTutorialWalkthroughMenu::CheckTutorialCountdownTimer()
{
	// If countdown has reached 0, enter the next walkthrough
	if (TutorialCountdownStep == 0)
	{
		TutorialManager->AdvanceWalkthrough(false);
	}
	else
	{
		// Set timer again for 1 second
		UpdateCountdownText();
		TutorialCountdownStep--;
		GetWorld()->GetTimerManager().SetTimer(TutorialCountdownTimer, this, &UTutorialWalkthroughMenu::CheckTutorialCountdownTimer, 1.f);
	}
}

void UTutorialWalkthroughMenu::UpdateCountdownText()
{
	WalkthroughItemWidget->ButtonWalkthroughProceed->ButtonText->SetText(FText::FromString(CountdownPrefix + FString::FormatAsNumber(TutorialCountdownStep) + CountdownSuffix));
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

void UTutorialWalkthroughMenu::ToggleMediaPlayerInteraction(bool NewEnable)
{
	// Because tutorial video is streamed, use LoadingImage to mask previous video while waiting for new video to load
	// Prevent user interaction to media player while it is loading
	if (NewEnable)
	{
		WalkthroughItemWidget->LoadingImage->SetVisibility(ESlateVisibility::Collapsed);
		WalkthroughItemWidget->ButtonWalkthroughProceed->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		WalkthroughItemWidget->LoadingImage->SetVisibility(ESlateVisibility::Visible);
		WalkthroughItemWidget->ButtonWalkthroughProceed->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTutorialWalkthroughMenu::OnUrlMediaOpened(FString Url)
{
	ToggleMediaPlayerInteraction(true);
	if (MediaPlayer)
	{

		MediaPlayer->Play();
	}
}

void UTutorialWalkthroughMenu::OnUrlMediaOpenFailed(FString Url)
{
	ToggleMediaPlayerInteraction(true);
}

void UTutorialWalkthroughMenu::OnReleaseButtonIntroProceed()
{
	TutorialManager->AdvanceWalkthrough(false);
}

void UTutorialWalkthroughMenu::OnReleaseButtonIntroGoBack()
{
	TutorialManager->EndWalkthrough(true);
}

void UTutorialWalkthroughMenu::OnReleaseWalkthroughButtonProceed()
{
	bool bSkipped = !GetWorld()->GetTimerManager().IsTimerActive(TutorialCountdownTimer);
	TutorialManager->AdvanceWalkthrough(bSkipped);
}

void UTutorialWalkthroughMenu::OnReleaseButtonWalkthroughGoBack()
{
	TutorialManager->RewindWalkthrough();
}

void UTutorialWalkthroughMenu::OnReleaseButtonOutroProceed()
{
	TutorialManager->EndWalkthrough(false);

	// TODO: allow for advancing to "expert" in the future
	TutorialManager->OpenWalkthroughProject(EModumateWalkthroughCategories::Intermediate);
}

void UTutorialWalkthroughMenu::OnReleaseButtonOutroGoBack()
{
	TutorialManager->EndWalkthrough(false);

	FString mainMenuMap = UGameMapsSettings::GetGameDefaultMap();
	UGameplayStatics::OpenLevel(this, FName(*mainMenuMap));
}
