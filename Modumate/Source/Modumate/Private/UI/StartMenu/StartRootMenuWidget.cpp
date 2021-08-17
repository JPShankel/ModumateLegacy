// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartRootMenuWidget.h"

#include "Components/HorizontalBox.h"
#include "HAL/PlatformMisc.h"
#include "ModumateCore/PlatformFunctions.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "UI/StartMenu/StartBlockHomeWidget.h"
#include "UI/TutorialMenu/TutorialMenuWidget.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UnrealClasses/ModumateGameInstance.h"

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
	if (!(ButtonHelp && ButtonQuit && ButtonOpen && ModalStatusDialog))
	{
		return false;
	}

	ButtonHelp->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedHelp);
	ButtonQuit->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedQuit);
	ButtonOpen->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedOpen);
	ModalStatusDialog->SetVisibility(ESlateVisibility::Collapsed);
	bHasUserLoggedIn = false;

	UGameViewportClient* viewportClient = ModumateGameInstance ? ModumateGameInstance->GetGameViewportClient() : nullptr;
	if (viewportClient)
	{
		viewportClient->OnWindowCloseRequested().BindUObject(this, &UStartRootMenuWidget::ConfirmQuit);
	}

	return true;
}

void UStartRootMenuWidget::ShowStartMenu()
{
	if (Start_Home_BP && (Start_Home_BP->GetVisibility() == ESlateVisibility::Collapsed))
	{
		Start_Home_BP->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		// TODO: Play widget opening animation here
		Start_Home_BP->OpenRecentProjectMenu();
		Start_Home_BP->TutorialsMenuWidgetBP->BuildTutorialMenu();

		OpenCreateNewButtonsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UStartRootMenuWidget::ShowModalStatus(const FText& StatusText, bool bAllowDismiss)
{
	ModalStatusDialog->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ModalStatusDialog->ShowStatusDialog(LOCTEXT("StatusTitle", "STATUS"), StatusText, bAllowDismiss);
}

void UStartRootMenuWidget::HideModalStatus()
{
	ModalStatusDialog->CloseModalDialog();
	ModalStatusDialog->SetVisibility(ESlateVisibility::Collapsed);
}

bool UStartRootMenuWidget::ConfirmQuit()
{
	if (bShowingQuitConfirmation)
	{
		return false;
	}

	bShowingQuitConfirmation = true;

	FText quitConfirmMsg = LOCTEXT("QuitConfirmMessage", "Are you sure you want to quit Modumate?");
	FText quitConfirmCaption = LOCTEXT("QuitConfirmCaption", "Quit");
	auto confirmResponse = FPlatformMisc::MessageBoxExt(EAppMsgType::OkCancel, *quitConfirmMsg.ToString(), *quitConfirmCaption.ToString());

	bShowingQuitConfirmation = false;
	return (confirmResponse == EAppReturnType::Ok);
}

void UStartRootMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UStartRootMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Move on to the next step once the user is logged in
	if (!bHasUserLoggedIn && ModumateGameInstance && (ModumateGameInstance->LoginStatus() == ELoginStatus::Connected))
	{
		bHasUserLoggedIn = true;
		auto* mainMenuGameMode = GetWorld()->GetAuthGameMode<AMainMenuGameMode>();
		if (ensure(mainMenuGameMode))
		{
			mainMenuGameMode->OnLoggedIn();
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

#undef LOCTEXT_NAMESPACE
