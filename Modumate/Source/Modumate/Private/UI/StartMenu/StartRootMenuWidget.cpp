// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartRootMenuWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/StartMenu/StartBlockHomeWidget.h"
#include "HAL/PlatformMisc.h"
#include "Components/HorizontalBox.h"
#include "UnrealClasses/MainMenuGameMode_CPP.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"

UStartRootMenuWidget::UStartRootMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, UserWidgetPool(*this)
{
}

bool UStartRootMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	ModumateGameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	if (!(ButtonHelp && ButtonQuit && ButtonOpen && ButtonCreateNew))
	{
		return false;
	}

	ButtonHelp->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedHelp);
	ButtonQuit->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedQuit);
	ButtonOpen->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedOpen);
	ButtonCreateNew->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedCreateNew);

	return true;
}

void UStartRootMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UStartRootMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Open project menu when user is connected
	if (ModumateGameInstance && ModumateGameInstance->LoginStatus() == ELoginStatus::Connected)
	{
		if (Start_Home_BP && Start_Home_BP->GetVisibility() == ESlateVisibility::Collapsed)
		{
			ShowStartMenu();
		}
	}
}

void UStartRootMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	UserWidgetPool.ReleaseAllSlateResources();
}

void UStartRootMenuWidget::OnButtonReleasedQuit()
{
	FPlatformMisc::RequestExit(false);
}

void UStartRootMenuWidget::OnButtonReleasedHelp()
{
	FPlatformProcess::LaunchURL(*HelpURL, nullptr, nullptr);
}

void UStartRootMenuWidget::OnButtonReleasedOpen()
{
	AMainMenuGameMode_CPP* mainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode_CPP>();
	if (mainMenuGameMode)
	{
		mainMenuGameMode->OpenProjectFromPicker();
	}
}

void UStartRootMenuWidget::OnButtonReleasedCreateNew()
{
	UGameplayStatics::OpenLevel(this, NewLevelName, true);
}

void UStartRootMenuWidget::ShowStartMenu()
{
	Start_Home_BP->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// TODO: Play widget opening animation here
	Start_Home_BP->OpenRecentProjectMenu();
	Start_Home_BP->TutorialsMenuWidgetBP->BuildTutorialMenu(Start_Home_BP->TutorialsMenuWidgetBP->TestTutorialInfo);

	OpenCreateNewButtonsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
