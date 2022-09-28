// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateWebBrowser.h"
#include "Runtime/WebBrowser/Public/SWebBrowser.h"
#include "UnrealClasses/EditModelPlayerController.h"

const FName UModumateWebBrowser::MODUMATE_WEB_TAG = FName(TEXT("ModumateWebBrowser"));

void UModumateWebBrowser::CallBindUObject(const FString& Name, UObject* Object, bool bIsPermanent)
{
	if (WebBrowserWidget.IsValid())
	{
		WebBrowserWidget->BindUObject(Name, Object, bIsPermanent);
	}
}

void UModumateWebBrowser::SetPlayerInputPosition(const FVector2D& position)
{
	WebBrowserWidget->SetPlayerInputPosition(position);
}

TSharedRef<SWidget> UModumateWebBrowser::RebuildWidget()
{
	// Super::RebuildWidget() creates SWebBrowser with binding delegates.
	// To avoid engine change, bind Modumate delegates here

	if (IsDesignTime())
	{
		return Super::RebuildWidget();
	}
	else
	{
		WebBrowserWidget = SNew(SWebBrowser)
			.InitialURL(InitialURL)
			.ShowControls(false)
			.ShowAddressBar(false)
			.BrowserFrameRate(30)
			.SupportsTransparency(bSupportsTransparency)
			.OnUrlChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnUrlChanged))
			.OnBeforePopup(BIND_UOBJECT_DELEGATE(FOnBeforePopupDelegate, HandleOnBeforePopup))
			.OnLoadCompleted(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadCompleted))
			.OnLoadError(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadError))
			.OnLoadStarted(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadStarted));
		
		WebBrowserWidget->SetTag(MODUMATE_WEB_TAG);
		
		return WebBrowserWidget.ToSharedRef();
	}
}

void UModumateWebBrowser::HandleOnLoadCompleted()
{
	if(!WebReloadWatchdog)
	{
		WebReloadWatchdog = NewObject<UWebWatchdog>();
		WebReloadWatchdog->Rename(nullptr, this);
		WebReloadWatchdog->OnWatchdogExpired().AddUObject(this, &UModumateWebBrowser::WatchdogTriggered);
		CallBindUObject(TEXT("watchdog"), WebReloadWatchdog, true);
		
	}	
	OnLoadCompleted.Broadcast();
}

void UModumateWebBrowser::HandleOnLoadError()
{
	OnLoadError.Broadcast();
}

void UModumateWebBrowser::HandleOnLoadStarted()
{
	OnLoadStarted.Broadcast();
}

void UModumateWebBrowser::WatchdogTriggered()
{
	FString error = TEXT("Web Watchdog Triggered, reloading");
	UE_LOG(LogTemp, Warning, TEXT("%s"), *error);
	
	auto controller = GetOwningLocalPlayer<AEditModelPlayerController>();
	if (controller)
	{
		auto pc = GetOwningLocalPlayer<AEditModelPlayerController>();
		if(pc)
		{
			pc->PostServerLog(error);	
		}
	}

	if (WebBrowserWidget)
	{
		WebBrowserWidget->Reload();
	}
}