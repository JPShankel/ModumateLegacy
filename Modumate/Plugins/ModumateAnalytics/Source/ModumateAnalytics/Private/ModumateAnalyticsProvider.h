// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"

#include "ModumateAnalyticsProvider.generated.h"

class Error;
class FModumateCloudConnection;

USTRUCT()
struct FModumateAnalyticsEventData
{
	GENERATED_BODY()

	UPROPERTY()
	FString key;

	UPROPERTY()
	FString appVersion;

	UPROPERTY()
	float value;

	UPROPERTY()
	int64 timestamp;

	UPROPERTY()
	bool inTutorial;

	UPROPERTY()
	FString sessionId;

	UPROPERTY()
	FString stringValue;

	FModumateAnalyticsEventData();
	FModumateAnalyticsEventData(const FString& EventName, const FString& AppVersion, const TArray<FAnalyticsEventAttribute>& Attributes);
};

USTRUCT()
struct FModumateAnalyticsEventArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FModumateAnalyticsEventData> Events;
};

class FAnalyticsProviderModumate :
	public IAnalyticsProvider
{
	/** Tracks whether we need to start the session or restart it */
	bool bHasSessionStarted;
	/** Id representing the user the analytics are recording for */
	FString UserId;
	/** Unique Id representing the session the analytics are recording for */
	FString SessionId;
	/** Holds the build info if set */
	FString BuildInfo;
	/** All of the analytics events that have been recorded so far */
	TArray<FModumateAnalyticsEventData> AllEvents;
	/** The analytics events that have been recorded but not yet sent so far */
	TArray<FModumateAnalyticsEventData> EventBuffer;

	TSharedPtr<FModumateCloudConnection> CloudConnection;

	static const int32 MaxEventBufferLength;

public:
	FAnalyticsProviderModumate();
	virtual ~FAnalyticsProviderModumate();

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;

	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;

	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;

	virtual void SetBuildInfo(const FString& InBuildInfo) override;

protected:
	bool UploadBufferedEvents();
};
