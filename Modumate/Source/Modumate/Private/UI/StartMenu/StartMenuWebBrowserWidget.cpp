// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartMenuWebBrowserWidget.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UI/Custom/ModumateWebBrowser.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "Online/ModumateCloudConnection.h"

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
	ModumateWebBrowser->OnLoadCompleted.AddDynamic(this, &UStartMenuWebBrowserWidget::OnWebBrowserLoadCompleted);

	return true;
}

void UStartMenuWebBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	static const FString bindObjName(TEXT("obj"));
	ModumateWebBrowser->CallBindUObject(bindObjName, GetGameInstance(), true);

	LaunchModumateCloudWebsite();
}

void UStartMenuWebBrowserWidget::ShowModalStatus(const FText& StatusText, bool bAllowDismiss)
{
	ModalStatusDialog->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ModalStatusDialog->ShowStatusDialog(LOCTEXT("StatusTitle", "STATUS"), StatusText, bAllowDismiss);
}

void UStartMenuWebBrowserWidget::HideModalStatus()
{
	ModalStatusDialog->CloseModalDialog();
	ModalStatusDialog->SetVisibility(ESlateVisibility::Collapsed);
}

void UStartMenuWebBrowserWidget::OnWebBrowserLoadCompleted()
{
#if 0
	const auto projectSettings = GetDefault<UGeneralProjectSettings>();
	FString projectVersionJS = TEXT("'") + projectSettings->ProjectVersion + TEXT("'");

	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	FString uploadRefreshToken = cloudConnection.IsValid() ? cloudConnection->GetRefreshToken() : FString();
	FString uploadRefreshTokenJS = TEXT("'") + uploadRefreshToken + TEXT("'");

	FString toggleUEString = TEXT("toggleModumateUESwitch(") + projectVersionJS + TEXT(", ") + uploadRefreshTokenJS + TEXT(")");
	ModumateWebBrowser->ExecuteJavascript(toggleUEString);

	// TODO: Remove log
	UE_LOG(LogTemp, Log, TEXT("ExecuteJavascript: %s"), *toggleUEString);
#endif
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