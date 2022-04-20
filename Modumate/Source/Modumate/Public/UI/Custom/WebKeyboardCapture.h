// Copyright © 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateWebBrowser.h"
#include "WebBrowserViewport.h"
#include "Blueprint/UserWidget.h"

#include "WebKeyboardCapture.generated.h"

/**
 * 
 */
UCLASS()
class MODUMATE_API UWebKeyboardCapture : public UUserWidget
{
	GENERATED_BODY()

public:
	UWebKeyboardCapture(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;
	virtual void OnWidgetRebuilt() override;

protected:
	virtual bool NativeSupportsKeyboardFocus() const override;

	TSharedPtr<FWebBrowserViewport> WebBrowser() const;
	virtual FReply NativeOnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
};
