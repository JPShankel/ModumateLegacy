// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SplashMenu/SplashRootMenuWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/SplashMenu/SplashBlockHomeWidget.h"
#include "HAL/PlatformMisc.h"

USplashRootMenuWidget::USplashRootMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USplashRootMenuWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	ModumateGameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	if (!(ButtonHelp && ButtonQuit))
	{
		return false;
	}

	ButtonHelp->ModumateButton->OnReleased.AddDynamic(this, &USplashRootMenuWidget::OnButtonReleasedHelp);
	ButtonQuit->ModumateButton->OnReleased.AddDynamic(this, &USplashRootMenuWidget::OnButtonReleasedQuit);
	return true;
}

void USplashRootMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USplashRootMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Open project menu when user is connected
	if (ModumateGameInstance && ModumateGameInstance->LoginStatus() == ELoginStatus::Connected)
	{
		if (Splash_Home_BP && Splash_Home_BP->GetVisibility() == ESlateVisibility::Collapsed)
		{
			Splash_Home_BP->SetVisibility(ESlateVisibility::SelfHitTestInvisible); 
			// TODO: Play widget opening animation here
			Splash_Home_BP->OpenRecentProjectMenu();
		}
	}
}

void USplashRootMenuWidget::OnButtonReleasedQuit()
{
	FPlatformMisc::RequestExit(false);
}

void USplashRootMenuWidget::OnButtonReleasedHelp()
{
	FPlatformProcess::LaunchURL(*HelpURL, nullptr, nullptr);
}
