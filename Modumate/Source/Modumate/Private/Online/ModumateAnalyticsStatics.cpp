// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAnalyticsStatics.h"

#include "Analytics.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Database/ModumateObjectEnums.h"
#include "UnrealClasses/Modumate.h"
#include "UnrealClasses/ModumateGameInstance.h"


// Made to be consistent with those interpreted by FAnalyticsProviderRevulytics::RecordEvent
const FString UModumateAnalyticsStatics::AttrNameCategory(TEXT("Category"));
const FString UModumateAnalyticsStatics::AttrNameCustomValue(TEXT("CustomValue"));
const FString UModumateAnalyticsStatics::AttrNameInTutorial(TEXT("InTutorial"));

FOnModumateAnalyticsEvent UModumateAnalyticsStatics::OnRecordedAnalyticsEvent;
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

bool UModumateAnalyticsStatics::RecordEventSimple(UObject* WorldContextObject, EModumateAnalyticsCategory EventCategory, const FString &EventName)
{
	if (IAnalyticsProvider *analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		TArray<FAnalyticsEventAttribute> eventAttributes;
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCategory, GetEnumValueString(EventCategory)));
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameInTutorial, UModumateAnalyticsStatics::bInTutorial));

		analytics->RecordEvent(EventName, eventAttributes);
		UModumateAnalyticsStatics::OnRecordedAnalyticsEvent.Broadcast(EventCategory, EventName);

		return true;
	}

	return false;
}

bool UModumateAnalyticsStatics::RecordEventCustomFloat(UObject* WorldContextObject, EModumateAnalyticsCategory EventCategory, const FString &EventName, float CustomValue)
{
	if (IAnalyticsProvider *analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		TArray<FAnalyticsEventAttribute> eventAttributes;
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCategory, GetEnumValueString(EventCategory)));
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameCustomValue, CustomValue));
		eventAttributes.Add(FAnalyticsEventAttribute(UModumateAnalyticsStatics::AttrNameInTutorial, UModumateAnalyticsStatics::bInTutorial));

		analytics->RecordEvent(EventName, eventAttributes);
		UModumateAnalyticsStatics::OnRecordedAnalyticsEvent.Broadcast(EventCategory, EventName);

		return true;
	}

	return false;
}

bool UModumateAnalyticsStatics::RecordToolUsage(UObject* WorldContextObject, EToolMode ToolMode, float UsedDuration)
{
	FString toolModeEnumString = GetEnumValueString(ToolMode);
	FString eventName = FString::Printf(TEXT("TimeUsedTool_%s"), *toolModeEnumString.RightChop(3));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Tools, eventName, UsedDuration);
}

bool UModumateAnalyticsStatics::RecordSimpleToolEvent(UObject* WorldContextObject, EToolMode ToolMode, const FString& SubEventName)
{
	FString toolModeEnumString = GetEnumValueString(ToolMode);
	FString eventName = FString::Printf(TEXT("%s_%s"), *toolModeEnumString.RightChop(3), *SubEventName);
	return UModumateAnalyticsStatics::RecordEventSimple(WorldContextObject, EModumateAnalyticsCategory::Tools, eventName);
}

bool UModumateAnalyticsStatics::RecordObjectCreation(UObject* WorldContextObject, EObjectType ObjectType, int32 Count)
{
	FString objectEnumString = GetEnumValueString(ObjectType);
	FString eventName = FString::Printf(TEXT("CreatedObject_%s"), *objectEnumString.RightChop(2));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Objects, eventName, Count);
}

bool UModumateAnalyticsStatics::RecordObjectDeletion(UObject* WorldContextObject, EObjectType ObjectType, int32 Count)
{
	FString objectEnumString = GetEnumValueString(ObjectType);
	FString eventName = FString::Printf(TEXT("DeletedObject_%s"), *objectEnumString.RightChop(2));
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Objects, eventName, Count);
}

bool UModumateAnalyticsStatics::RecordSessionDuration(UObject* WorldContextObject, const FTimespan& SessionDuration)
{
	static const FString eventName(TEXT("SessionDuration"));
	bool bEventSuccess = UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Session, eventName, SessionDuration.GetTotalSeconds());

	if (IAnalyticsProvider* analytics = GetAnalyticsFromWorld(WorldContextObject))
	{
		analytics->FlushEvents();
	}

	return bEventSuccess;
}

bool UModumateAnalyticsStatics::RecordPresetCreation(UObject* WorldContextObject, const FBIMPresetInstance* PresetInstance)
{
	FString eventName = UModumateAnalyticsStatics::GetPresetEventName(TEXT("Create"), PresetInstance);
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Presets, eventName, 1.0f);
}

bool UModumateAnalyticsStatics::RecordPresetUpdate(UObject* WorldContextObject, const FBIMPresetInstance* PresetInstance)
{
	FString eventName = UModumateAnalyticsStatics::GetPresetEventName(TEXT("Update"), PresetInstance);
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Presets, eventName, 1.0f);
}

bool UModumateAnalyticsStatics::RecordPresetDeletion(UObject* WorldContextObject, const FBIMPresetInstance* PresetInstance)
{
	FString eventName = UModumateAnalyticsStatics::GetPresetEventName(TEXT("Delete"), PresetInstance);
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Presets, eventName, 1.0f);
}

bool UModumateAnalyticsStatics::RecordPresetDuplication(UObject* WorldContextObject, const FBIMPresetInstance* PresetInstance)
{
	FString eventName = UModumateAnalyticsStatics::GetPresetEventName(TEXT("Duplicate"), PresetInstance);
	return UModumateAnalyticsStatics::RecordEventCustomFloat(WorldContextObject, EModumateAnalyticsCategory::Presets, eventName, 1.0f);
}


FString UModumateAnalyticsStatics::GetPresetEventName(const TCHAR* Prefix, const FBIMPresetInstance* PresetInstance)
{
	FString nodeScopeString = GetEnumValueString(PresetInstance ? PresetInstance->NodeScope : EBIMValueScope::None);
	FString objectTypeString = GetEnumValueString(PresetInstance ? PresetInstance->ObjectType : EObjectType::OTNone).RightChop(2);
	return FString::Printf(TEXT("%s_%s_%s"), Prefix, *nodeScopeString, *objectTypeString);
}
