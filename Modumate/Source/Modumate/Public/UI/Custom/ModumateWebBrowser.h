// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WebBrowserWidget/Public/WebBrowser.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Online/WebWatchdog.h"
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
	void CallBindUObject(const FString& Name, UObject* Object);
	void SetPlayerInputPosition(const FVector2D& position);
	static const FName MODUMATE_WEB_TAG;

protected:
	typedef struct
	{
		FString Name;
		UObject* Object;
	} BoundObjectRequest;

	TArray<BoundObjectRequest> BoundObjects;
	
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void HandleOnLoadCompleted();
	void HandleOnLoadError();
	void HandleOnLoadStarted();

	UPROPERTY()
	UWebWatchdog* WebReloadWatchdog;

	UFUNCTION()
	void WatchdogTriggered();
};