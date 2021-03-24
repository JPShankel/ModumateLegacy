// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartRootMenuWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/StartMenu/StartBlockHomeWidget.h"
#include "HAL/PlatformMisc.h"
#include "Components/HorizontalBox.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "ModumateCore/PlatformFunctions.h"

#define LOCTEXT_NAMESPACE "ModumateRootMenuWidget"

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
	if (!(ButtonHelp && ButtonQuit && ButtonOpen))
	{
		return false;
	}

	ButtonHelp->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedHelp);
	ButtonQuit->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedQuit);
	ButtonOpen->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedOpen);

	UGameViewportClient* viewportClient = ModumateGameInstance ? ModumateGameInstance->GetGameViewportClient() : nullptr;
	if (viewportClient)
	{
		viewportClient->OnWindowCloseRequested().BindUObject(this, &UStartRootMenuWidget::ConfirmQuit);
	}

	return true;
}

bool UStartRootMenuWidget::ConfirmQuit() const
{
	FText quitConfirmMsg = LOCTEXT("QuitConfirmMessage", "Are you sure you want to quit Modumate?");
	FText quitConfirmCaption = LOCTEXT("QuitConfirmCaption", "Quit");
	auto confirmResponse = Modumate::PlatformFunctions::ShowMessageBox(quitConfirmMsg.ToString(), quitConfirmCaption.ToString(), Modumate::PlatformFunctions::YesNo);
	return confirmResponse == Modumate::PlatformFunctions::EMessageBoxResponse::Yes;
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
	if (ConfirmQuit())
	{
		FPlatformMisc::RequestExit(false);
	}
}

void UStartRootMenuWidget::OnButtonReleasedHelp()
{
	FPlatformProcess::LaunchURL(*HelpURL, nullptr, nullptr);
}

void UStartRootMenuWidget::OnButtonReleasedOpen()
{
	AMainMenuGameMode* mainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode>();
	if (mainMenuGameMode)
	{
		mainMenuGameMode->OpenProjectFromPicker();
	}
}

void UStartRootMenuWidget::ShowStartMenu()
{
	if (!GetWorld()->GetGameInstance<UModumateGameInstance>()->TutorialManager->CheckAbsoluteBeginner())
	{
		Start_Home_BP->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		// TODO: Play widget opening animation here
		Start_Home_BP->OpenRecentProjectMenu();
		Start_Home_BP->TutorialsMenuWidgetBP->BuildTutorialMenu();

		OpenCreateNewButtonsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

#undef LOCTEXT_NAMESPACE
