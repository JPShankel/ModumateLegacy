// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/StartMenu/StartMenuWebBrowserWidget.h"

#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuGameMode.h"

#include "UI/Custom/ModumateWebBrowser.h"

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
}

void UStartMenuWebBrowserWidget::OnWebBrowserLoadCompleted()
{
	const auto projectSettings = GetDefault<UGeneralProjectSettings>();
	FString toggleUEString = TEXT("toggleModumateUESwitch('") + projectSettings->ProjectVersion + TEXT("')");
	ModumateWebBrowser->ExecuteJavascript(toggleUEString);
}
