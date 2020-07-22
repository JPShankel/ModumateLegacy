// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartRootMenuWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/StartMenu/StartBlockHomeWidget.h"
#include "HAL/PlatformMisc.h"

UStartRootMenuWidget::UStartRootMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStartRootMenuWidget::Initialize()
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

	ButtonHelp->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedHelp);
	ButtonQuit->ModumateButton->OnReleased.AddDynamic(this, &UStartRootMenuWidget::OnButtonReleasedQuit);
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
			Start_Home_BP->SetVisibility(ESlateVisibility::SelfHitTestInvisible); 
			// TODO: Play widget opening animation here
			Start_Home_BP->OpenRecentProjectMenu();
		}
	}
}

void UStartRootMenuWidget::OnButtonReleasedQuit()
{
	FPlatformMisc::RequestExit(false);
}

void UStartRootMenuWidget::OnButtonReleasedHelp()
{
	FPlatformProcess::LaunchURL(*HelpURL, nullptr, nullptr);
}
