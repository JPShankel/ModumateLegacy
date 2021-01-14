// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAnalyticsStatics.h"

#include "Analytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "UnrealClasses/Modumate.h"
#include "UnrealClasses/ModumateGameInstance.h"


// Made to be consistent with those interpreted by FAnalyticsProviderRevulytics::RecordEvent
const FString UModumateAnalyticsStatics::AttrNameCategory(TEXT("Category"));
const FString UModumateAnalyticsStatics::AttrNameCustomValue(TEXT("CustomValue"));
const FString UModumateAnalyticsStatics::AttrNameInTutorial(TEXT("InTutorial"));

const FString UModumateAnalyticsStatics::EventCategoryObjects(TEXT("Objects"));
const FString UModumateAnalyticsStatics::EventCategoryTools(TEXT("Tools"));
const FString UModumateAnalyticsStatics::EventCategoryPresets(TEXT("Presets"));
const FString UModumateAnalyticsStatics::EventCategoryView(TEXT("View"));
const FString UModumateAnalyticsStatics::EventCategorySession(TEXT("Session"));

bool UModumateAnalyticsStatics::bInTutorial = false;

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
			analyticsInstance->SetBuildInfo(FModumateModule::Get().GetProjectDisplayVersion());
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

void UModumateAnalyticsStatics::SetInTutorial(bool bNewIntutorial)
{
	UModumateAnalyticsStatics::bInTutorial = bNewIntutorial;
}

bool UModumateAnalyticsStatics::RecordEventSimple(UObject* WorldContextObject, const FString &EventCategory, const FString &EventName)
{
	if (IAnalyticsProvider *analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		TArray<FAnalyticsEventAttribute> eventAttributes;
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCategory, EventCategory));
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameInTutorial, UModumateAnalyticsStatics::bInTutorial));

		analytics->RecordEvent(EventName, eventAttributes);
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
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameInTutorial, UModumateAnalyticsStatics::bInTutorial));

		analytics->RecordEvent(EventName, eventAttributes);
		return true;
	}

	return false;
}

bool UModumateAnalyticsStatics::RecordToolUsage(UObject* WorldContextObject, EToolMode ToolMode, float UsedDuration)
{
	FString toolModeEnumString = EnumValueString(EToolMode, ToolMode);
	FString eventName = FString::Printf(TEXT("TimeUsedTool_%s"), *toolModeEnumString.RightChop(3));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EventCategoryTools, eventName, UsedDuration);
}

bool UModumateAnalyticsStatics::RecordObjectCreation(UObject* WorldContextObject, EObjectType ObjectType)
{
	FString objectEnumString = EnumValueString(EObjectType, ObjectType);
	FString eventName = FString::Printf(TEXT("CreatedObject_%s"), *objectEnumString.RightChop(2));
	return UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, EventCategoryObjects, eventName);
}

bool UModumateAnalyticsStatics::RecordObjectDeletion(UObject* WorldContextObject, EObjectType ObjectType)
{
	FString objectEnumString = EnumValueString(EObjectType, ObjectType);
	FString eventName = FString::Printf(TEXT("DeletedObject_%s"), *objectEnumString.RightChop(2));
	return UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, EventCategoryObjects, eventName);
}

bool UModumateAnalyticsStatics::RecordSessionDuration(UObject* WorldContextObject, const FTimespan& SessionDuration)
{
	static const FString eventName(TEXT("SessionDuration"));
	bool bEventSuccess = UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EventCategorySession, eventName, SessionDuration.GetTotalSeconds());

	if (IAnalyticsProvider* analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		analytics->FlushEvents();
	}

	return bEventSuccess;
}

bool UModumateAnalyticsStatics::RecordPresetCreation(UObject* WorldContextObject)
{
	static const FString eventName(TEXT("Create"));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EventCategoryPresets, eventName, 1.0f);
}

bool UModumateAnalyticsStatics::RecordPresetUpdate(UObject* WorldContextObject)
{
	static const FString eventName(TEXT("Update"));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EventCategoryPresets, eventName, 1.0f);
}

bool UModumateAnalyticsStatics::RecordPresetDeletion(UObject* WorldContextObject)
{
	static const FString eventName(TEXT("Delete"));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EventCategoryPresets, eventName, 1.0f);
}
