// Copyright © 2021 Modumate, Inc. All Rights Reserved.


#include "UI/Custom/WebKeyboardCapture.h"

#include "SWebBrowser.h"
#include "WebBrowserViewport.h"
#include "UI/EditModelUserWidget.h"
#include "UI/DrawingDesigner/DrawingDesignerWebBrowserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"

FReply UWebKeyboardCapture::NativeOnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharEvent)
{
	auto wb = WebBrowser();
	if(wb)
	{
		return WebBrowser()->OnKeyChar(InGeometry, InCharEvent);
	}
	else
	{
		return UUserWidget::NativeOnKeyChar(InGeometry, InCharEvent);
	}
}

FReply UWebKeyboardCapture::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	auto wb = WebBrowser();
	if(wb)
	{
		return WebBrowser()->OnKeyDown(InGeometry, InKeyEvent);
	}
	else
	{
		return UUserWidget::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
}

FReply UWebKeyboardCapture::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	auto wb = WebBrowser();
	if(wb)
	{
		return WebBrowser()->OnKeyUp(InGeometry, InKeyEvent);
	}
	else
	{
		return UUserWidget::NativeOnKeyUp(InGeometry, InKeyEvent);
	}
}

UWebKeyboardCapture::UWebKeyboardCapture(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	
}

bool UWebKeyboardCapture::Initialize()
{
	return Super::Initialize();
}

void UWebKeyboardCapture::OnWidgetRebuilt()
{
	TSharedRef<SWidget> slateWidget = TakeWidget();
	slateWidget->SetTag(UModumateWebBrowser::MODUMATE_WEB_TAG);
}

bool UWebKeyboardCapture::NativeSupportsKeyboardFocus() const
{
	return true;
}

TSharedPtr<FWebBrowserViewport> UWebKeyboardCapture::WebBrowser() const
{
	const AEditModelPlayerController* playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if(playerController && playerController->EditModelUserWidget)
	{
		return playerController->EditModelUserWidget->DrawingDesigner->DrawingSetWebBrowser->WebBrowserWidget->BrowserView->BrowserViewport;	
	}
	return nullptr;
}
