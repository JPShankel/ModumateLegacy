// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WebBrowserWidget/Public/WebBrowser.h"
#include "ModumateWebBrowser.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateWebBrowser : public UWebBrowser
{
	GENERATED_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadCompleted);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadError);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadStarted);

	UPROPERTY(BlueprintAssignable)
	FOnLoadCompleted OnLoadCompleted;

	UPROPERTY(BlueprintAssignable)
	FOnLoadError OnLoadError;

	UPROPERTY(BlueprintAssignable)
	FOnLoadStarted OnLoadStarted;

	// Bind UObject to slate web browser
	UFUNCTION(BlueprintCallable)
	void CallBindUObject(const FString& Name, UObject* Object, bool bIsPermanent);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

	void HandleOnLoadCompleted();
	void HandleOnLoadError();
	void HandleOnLoadStarted();
};