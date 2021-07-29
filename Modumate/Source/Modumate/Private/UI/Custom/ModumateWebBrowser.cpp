// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateWebBrowser.h"
#include "Runtime/WebBrowser/Public/SWebBrowser.h"


void UModumateWebBrowser::CallBindUObject(const FString& Name, UObject* Object, bool bIsPermanent)
{
	if (WebBrowserWidget.IsValid())
	{
		WebBrowserWidget->BindUObject(Name, Object, bIsPermanent);
	}
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
			.SupportsTransparency(bSupportsTransparency)
			.OnUrlChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnUrlChanged))
			.OnBeforePopup(BIND_UOBJECT_DELEGATE(FOnBeforePopupDelegate, HandleOnBeforePopup))
			.OnLoadCompleted(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadCompleted))
			.OnLoadError(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadError))
			.OnLoadStarted(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadStarted));

		return WebBrowserWidget.ToSharedRef();
	}
}

void UModumateWebBrowser::HandleOnLoadCompleted()
{
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