// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartMenuWebBrowserWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UStartMenuWebBrowserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStartMenuWebBrowserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateWebBrowser* ModumateWebBrowser;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModalDialogWidget* ModalStatusDialog;

	void ShowModalStatus(const FText& StatusText, bool bAllowDismiss);
	void HideModalStatus();

	UFUNCTION()
	void OnWebBrowserLoadCompleted();

	UFUNCTION(BlueprintCallable)
	void LaunchModumateCloudWebsite();

	void LaunchURL(const FString& InURL);
};
