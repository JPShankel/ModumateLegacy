// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAnalyticsStatics.h"

#include "Analytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "UnrealClasses/ModumateGameInstance.h"


// Made to be consistent with those interpreted by FAnalyticsProviderRevulytics::RecordEvent
const FString UModumateAnalyticsStatics::AttrNameCategory(TEXT("Category"));
const FString UModumateAnalyticsStatics::AttrNameCustomValue(TEXT("CustomValue"));

TSharedPtr<IAnalyticsProvider> UModumateAnalyticsStatics::InitAnalytics()
{
	TSharedPtr<IAnalyticsProvider> analyticsInstance;

	// Mostly follows FAnalytics::Get().GetDefaultConfiguredProvider(), but allows overwriting
	// the provider based on the build configuration, so it can be enabled/disabled during development.
	FName providerModuleName = FAnalytics::ConfigFromIni::GetDefaultProviderModuleName();

	FAnalytics::ConfigFromIni analyticsConfig;
	FString overrideModuleString;
	if (GConfig->GetString(*analyticsConfig.SectionName, TEXT("ProviderModuleName"),
		overrideModuleString, analyticsConfig.IniName))
	{
		providerModuleName = FName(*overrideModuleString);
	}

	if (!providerModuleName.IsNone())
	{
		analyticsInstance = FAnalytics::Get().CreateAnalyticsProvider(
			providerModuleName,
			FAnalyticsProviderConfigurationDelegate::CreateRaw(
				&analyticsConfig,
				&FAnalytics::ConfigFromIni::GetValue)
		);

		if (analyticsInstance.IsValid())
		{
			analyticsInstance->StartSession();
		}
	}

	return analyticsInstance;
}

void UModumateAnalyticsStatics::ShutdownAnalytics(TSharedPtr<IAnalyticsProvider> &AnalyticsInstance)
{
	if (AnalyticsInstance.IsValid())
	{
		AnalyticsInstance.Get()->FlushEvents();
		AnalyticsInstance.Get()->EndSession();
		AnalyticsInstance.Reset();
	}
}

IAnalyticsProvider *UModumateAnalyticsStatics::GetAnalyticsFromWorld(UObject* WorldContextObject)
{
	UWorld *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UModumateGameInstance *gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	return gameInstance ? gameInstance->GetAnalytics() : nullptr;
}

bool UModumateAnalyticsStatics::RecordEventSimple(UObject* WorldContextObject, const FString &EventCategory, const FString &EventName)
{
	if (IAnalyticsProvider *analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		analytics->RecordEvent(EventName, UModumateAnalyticsStatics::AttrNameCategory, EventCategory);
		return true;
	}

	return false;
}

bool UModumateAnalyticsStatics::RecordEventCustomFloat(UObject* WorldContextObject, const FString &EventCategory, const FString &EventName, float CustomValue)
{
	if (IAnalyticsProvider *analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		TArray<FAnalyticsEventAttribute> eventAttributes;
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCategory, EventCategory));
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCustomValue, CustomValue));

		analytics->RecordEvent(EventName, eventAttributes);
		return true;
	}

	return false;
}

bool UModumateAnalyticsStatics::RecordEventCustomString(UObject* WorldContextObject, const FString &EventCategory, const FString &EventName, const FString &CustomValue)
{
	if (IAnalyticsProvider *analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		TArray<FAnalyticsEventAttribute> eventAttributes;
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCategory, EventCategory));
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCustomValue, CustomValue));

		analytics->RecordEvent(EventName, eventAttributes);
		return true;
	}

	return false;
}

bool UModumateAnalyticsStatics::RecordToolUsage(UObject* WorldContextObject, EToolMode ToolMode, float UsedDuration)
{
	static const FString eventCategory(TEXT("Tools"));

	FString toolModeEnumString = EnumValueString(EToolMode, ToolMode);
	FString eventName = FString::Printf(TEXT("TimeUsedTool_%s"), *toolModeEnumString.RightChop(3));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, eventCategory, eventName, UsedDuration);
}

bool UModumateAnalyticsStatics::RecordObjectCreation(UObject* WorldContextObject, EObjectType ObjectType)
{
	static const FString eventCategory(TEXT("Objects"));

	FString objectEnumString = EnumValueString(EObjectType, ObjectType);
	FString eventName = FString::Printf(TEXT("CreatedObject_%s"), *objectEnumString.RightChop(2));
	return UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, eventCategory, eventName);
}

bool UModumateAnalyticsStatics::RecordObjectDeletion(UObject* WorldContextObject, EObjectType ObjectType)
{
	static const FString eventCategory(TEXT("Objects"));

	FString objectEnumString = EnumValueString(EObjectType, ObjectType);
	FString eventName = FString::Printf(TEXT("DeletedObject_%s"), *objectEnumString.RightChop(2));
	return UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, eventCategory, eventName);
}

bool UModumateAnalyticsStatics::RecordSessionDuration(UObject* WorldContextObject, const FTimespan& SessionDuration)
{
	static const FString eventCategory(TEXT("Metrics"));
	static const FString eventName(TEXT("SessionDuration"));

	bool bEventSuccess = UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, eventCategory, eventName, SessionDuration.GetSeconds());

	if (IAnalyticsProvider* analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		analytics->FlushEvents();
	}

	return bEventSuccess;
}

bool UModumateAnalyticsStatics::RecordPresetCreation(UObject* WorldContextObject)
{
	static const FString eventCategory(TEXT("Presets"));
	static const FString eventName(TEXT("PresetCreation"));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, eventCategory, eventName, 1.0f);
}

bool UModumateAnalyticsStatics::RecordPresetUpdate(UObject* WorldContextObject)
{
	static const FString eventCategory(TEXT("Presets"));
	static const FString eventName(TEXT("PresetUpdate"));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, eventCategory, eventName, 1.0f);
}

bool UModumateAnalyticsStatics::RecordPresetDeletion(UObject* WorldContextObject)
{
	static const FString eventCategory(TEXT("Presets"));
	static const FString eventName(TEXT("PresetDeletion"));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, eventCategory, eventName, 1.0f);
}
