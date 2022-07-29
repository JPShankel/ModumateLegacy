// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "WebWatchdog.generated.h"

UCLASS()
class MODUMATE_API UWebWatchdog : public UObject
{
	GENERATED_BODY()
public:
	UWebWatchdog();

	UFUNCTION()
	void Ping();

	UFUNCTION()
	void Start();

	UFUNCTION()
	void Stop();
	
	DECLARE_EVENT(UWebWatchdog, FWatchdogExpiredEvent)
	FWatchdogExpiredEvent& OnWatchdogExpired();
	
protected:
	
	UFUNCTION()
	void Trigger();
	
	float WEB_WATCHDOG_TIMER_SECONDS = 5.0f;
	FTimerHandle WatchdogTimerHandle;

	FWatchdogExpiredEvent ExpiredEvent;
	UWorld* MyWorld;
	
};
