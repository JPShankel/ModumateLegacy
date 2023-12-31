// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateAnalytics.h"

#include "AnalyticsEventAttribute.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ModumateAnalyticsProvider.h"

#include "Online/ModumateAnalyticsStatics.h"
#include "Online/ModumateCloudConnection.h"
#include "UnrealClasses/ModumateGameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogModumateAnalytics, Display, All);

IMPLEMENT_MODULE( FAnalyticsModumate, ModumateAnalytics )


FModumateAnalyticsEventData::FModumateAnalyticsEventData()
	: key()
	, value(0)
	, timestamp(0)
	, inTutorial(false)
{
}

FModumateAnalyticsEventData::FModumateAnalyticsEventData(const FString& EventName, const FString& AppVersion, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	appVersion = AppVersion;
	value = 1.0f;
	inTutorial = false;

	for (const FAnalyticsEventAttribute& attribute : Attributes)
	{
		const FString& attrName = attribute.GetName();
		const FString& attrValue = attribute.GetValue();

		if (attrName == UModumateAnalyticsStatics::AttrNameCategory)
		{
			key = attrValue / EventName;
		}
		else if (attrName == UModumateAnalyticsStatics::AttrNameCustomValue)
		{
			LexTryParseString(value, *attrValue);
		}
		else if (attrName == UModumateAnalyticsStatics::AttrNameInTutorial)
		{
			LexTryParseString(inTutorial, *attrValue);
		}
		else if (attrName == UModumateAnalyticsStatics::AttrNameSessionID)
		{
			sessionId = attrValue;
		}
		else if (attrName == UModumateAnalyticsStatics::AttrNameStringValue)
		{
			stringValue = attrValue;
		}
	}

	if (key.IsEmpty())
	{
		key = EventName;
	}

	timestamp = FDateTime::UtcNow().ToUnixTimestamp();
}


void FAnalyticsModumate::StartupModule()
{
	ModumateProvider = MakeShareable(new FAnalyticsProviderModumate());
}

void FAnalyticsModumate::ShutdownModule()
{
	if (ModumateProvider.IsValid())
	{
		ModumateProvider->EndSession();
	}
}

TSharedPtr<IAnalyticsProvider> FAnalyticsModumate::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	return ModumateProvider;
}

// Provider

const int32 FAnalyticsProviderModumate::MaxEventBufferLength = 32;

FAnalyticsProviderModumate::FAnalyticsProviderModumate()
	: bHasSessionStarted(false)
{
	UserId = FPlatformMisc::GetLoginId();
}

FAnalyticsProviderModumate::~FAnalyticsProviderModumate()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderModumate::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		EndSession();
		bHasSessionStarted = false;
	}

	UModumateGameInstance* gameInstance = nullptr;
	for (const FWorldContext& worldContext : GEngine->GetWorldContexts())
	{
		UWorld* world = worldContext.World();
		gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
		if (gameInstance)
		{
			break;
		}
	}

	if (gameInstance == nullptr)
	{
		return false;
	}

	CloudConnection = gameInstance->GetCloudConnection();
	bHasSessionStarted = CloudConnection.IsValid();

	return bHasSessionStarted;
}

void FAnalyticsProviderModumate::EndSession()
{
	FlushEvents();

	bHasSessionStarted = false;
}

void FAnalyticsProviderModumate::FlushEvents()
{
	UploadBufferedEvents();
}

void FAnalyticsProviderModumate::SetUserID(const FString& InUserID)
{
	if (!bHasSessionStarted)
	{
		UserId = InUserID;
		UE_LOG(LogModumateAnalytics, Display, TEXT("User is now (%s)"), *UserId);
	}
	else
	{
		// Log that we shouldn't switch users during a session
		UE_LOG(LogModumateAnalytics, Warning, TEXT("FAnalyticsProviderModumate::SetUserID called while a session is in progress. Ignoring."));
	}
}

FString FAnalyticsProviderModumate::GetUserID() const
{
	return UserId;
}

FString FAnalyticsProviderModumate::GetSessionID() const
{
	return SessionId;
}

bool FAnalyticsProviderModumate::SetSessionID(const FString& InSessionID)
{
	if (!bHasSessionStarted)
	{
		SessionId = InSessionID;
		UE_LOG(LogModumateAnalytics, Display, TEXT("Session is now (%s)"), *SessionId);
	}
	else
	{
		// Log that we shouldn't switch session ids during a session
		UE_LOG(LogModumateAnalytics, Warning, TEXT("FAnalyticsProviderModumate::SetSessionID called while a session is in progress. Ignoring."));
	}
	return !bHasSessionStarted;
}

void FAnalyticsProviderModumate::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (bHasSessionStarted)
	{
		FModumateAnalyticsEventData eventData(EventName, BuildInfo, Attributes);
		AllEvents.Add(eventData);
		EventBuffer.Add(eventData);

		if (EventBuffer.Num() >= FAnalyticsProviderModumate::MaxEventBufferLength)
		{
			UploadBufferedEvents();
		}

		UE_LOG(LogModumateAnalytics, Verbose, TEXT("Buffered analytics event (%s)"), *eventData.key);
	}
	else
	{
		UE_LOG(LogModumateAnalytics, Warning, TEXT("FAnalyticsProviderModumate::RecordEvent called before StartSession. Ignoring."));
	}
}

void FAnalyticsProviderModumate::SetBuildInfo(const FString& InBuildInfo)
{
	BuildInfo = InBuildInfo;
}

bool FAnalyticsProviderModumate::UploadBufferedEvents()
{
	int32 numEvents = EventBuffer.Num();
	if (!bHasSessionStarted || !CloudConnection.IsValid() || !CloudConnection->IsLoggedIn() || (numEvents == 0))
	{
		return false;
	}

	FModumateAnalyticsEventArray eventArray{ EventBuffer };
	auto eventsJson = FJsonObjectConverter::UStructToJsonObject(eventArray);
	auto eventsJsonArray = eventsJson->GetArrayField(TEXT("Events"));

	bool bUploadRequestSuccess = CloudConnection->UploadAnalyticsEvents(eventsJsonArray,
		[numEvents](bool bSuccess, const TSharedPtr<FJsonObject>&) { UE_LOG(LogModumateAnalytics, Verbose, TEXT("Successfully uploaded %d buffered analytics events."), numEvents); },
		[](int32 Code, const FString& Error) { UE_LOG(LogModumateAnalytics, Error, TEXT("Analytics event upload error code: %d, error message: %s"), Code, *Error); }
	);

	if (bUploadRequestSuccess)
	{
		EventBuffer.Reset();
	}

	return bUploadRequestSuccess;
}
