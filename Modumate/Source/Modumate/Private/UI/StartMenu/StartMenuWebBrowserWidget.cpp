// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartMenuWebBrowserWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UI/Custom/ModumateWebBrowser.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "Online/ModumateCloudConnection.h"
#include "Runtime/WebBrowser/Public/SWebBrowser.h"

#define LOCTEXT_NAMESPACE "StartMenuWebBrowserWidget"

UStartMenuWebBrowserWidget::UStartMenuWebBrowserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStartMenuWebBrowserWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UStartMenuWebBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	static const FString bindObjName(TEXT("obj"));
	ModumateWebBrowser->WebBrowserWidget->BindUObject(bindObjName, GetGameInstance());

	LaunchModumateCloudWebsite();
}

void UStartMenuWebBrowserWidget::ShowModalStatus(const FText& StatusText, bool bAllowDismiss)
{
	ModalStatusDialog->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TArray<FModalButtonParam> buttonParams;
	if (bAllowDismiss)
	{
		FModalButtonParam dismissButton(EModalButtonStyle::Default, LOCTEXT("DismissAlert", "Dismiss"), nullptr);
		buttonParams.Add(dismissButton);
	}
	ModalStatusDialog->CreateModalDialog(LOCTEXT("StatusTitle", "STATUS"), StatusText, buttonParams);
}

void UStartMenuWebBrowserWidget::HideModalStatus()
{
	ModalStatusDialog->CloseModalDialog();
	ModalStatusDialog->SetVisibility(ESlateVisibility::Collapsed);
}

void UStartMenuWebBrowserWidget::LaunchModumateCloudWebsite()
{
	// After log-in from embedded AMS, default to open project page
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (cloudConnection)
	{
		UE_LOG(LogGameState, Log, TEXT("Launching project page from  cloud address: %s"), *cloudConnection->GetCloudProjectPageURL());
		ModumateWebBrowser->LoadURL(cloudConnection->GetCloudProjectPageURL());
	}
}

void UStartMenuWebBrowserWidget::LaunchURL(const FString& InURL)
{
	ModumateWebBrowser->LoadURL(InURL);
}

#undef LOCTEXT_NAMESPACE