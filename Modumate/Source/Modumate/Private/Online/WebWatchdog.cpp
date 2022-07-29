#include "Online/WebWatchdog.h"

#include "TimerManager.h"
#include "Engine/World.h"

UWebWatchdog::UWebWatchdog() : UObject()
{
	
}

void UWebWatchdog::Ping()
{
	Stop();
	Start();
}

void UWebWatchdog::Start()
{
	auto world = GetWorld();
	if(world)
	{
		world->GetTimerManager()
		.SetTimer(WatchdogTimerHandle, this, &UWebWatchdog::Trigger, WEB_WATCHDOG_TIMER_SECONDS);
	}
	
}

void UWebWatchdog::Stop()
{
	auto world = GetWorld();
	if(world)
	{
		world->GetTimerManager().ClearTimer(WatchdogTimerHandle);
	}
}

UWebWatchdog::FWatchdogExpiredEvent& UWebWatchdog::OnWatchdogExpired()
{
	return ExpiredEvent;
}

void UWebWatchdog::Trigger()
{
	ExpiredEvent.Broadcast();
}