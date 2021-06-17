// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "JsonUtilities.h"

class MODUMATE_API ICloudConnectionAutomation
{
public:
	virtual bool RecordRequest(FHttpRequestRef Request, int32 RequestIdx) = 0;
	virtual bool RecordResponse(FHttpRequestRef Request, int32 RequestIdx, bool bConnectionSuccess, int32 ResponseCode, const FString& ResponseContent) = 0;
	virtual bool GetResponse(FHttpRequestRef Request, int32 RequestIdx, bool& bOutSuccess, int32& OutCode, FString& OutContent, float& OutResponseTime) = 0;
	virtual FTimerManager& GetTimerManager() const = 0;
};

/**
 * Client to Cloud Connection Manager. Authentication details are passed automatically with all required requests.
 */
class MODUMATE_API FModumateCloudConnection : public TSharedFromThis<FModumateCloudConnection>
{
	public:
		FModumateCloudConnection();

		FString GetCloudRootURL() const;
		FString GetCloudAPIURL() const;

		const FString& GetAuthToken() const { return AuthToken; }
		void SetAuthToken(const FString& InAuthToken);

		const FString& GetXApiKey() const { return XApiKey; }
		void SetXApiKey(const FString& InXApiKey);

		static FString MakeEncryptionToken(const FString& UserID, const FString& ProjectID);
		static bool ParseEncryptionToken(const FString& EncryptionToken, FString& OutUserID, FString& OutProjectID);

		void CacheEncryptionKey(const FString& UserID, const FString& ProjectID, const FString& EncryptionKey);
		bool GetCachedEncryptionKey(const FString& UserID, const FString& ProjectID, FString& OutEncryptionKey);
		void QueryEncryptionKey(const FString& UserID, const FString& ProjectID, const FOnEncryptionKeyResponse& Delegate);

		void SetLoginStatus(ELoginStatus InLoginStatus);
		ELoginStatus GetLoginStatus() const;
		bool IsLoggedIn(bool bAllowCurrentLoggingIn = false) const;

		using FRequestCustomizer = TFunction<void(FHttpRequestRef& RefRequest)>;
		using FSuccessCallback = TFunction<void(bool,const TSharedPtr<FJsonObject>&)>;
		using FErrorCallback = TFunction<void(int32, const FString&)>;

		enum ERequestType { Get, Delete, Put, Post };
		bool RequestEndpoint(const FString& Endpoint, ERequestType RequestType, const FRequestCustomizer& Customizer, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback,
			bool bRefreshTokenOnAuthFailure = true);

		bool CreateReplay(const FString& SessionID, const FString& Version, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool UploadReplay(const FString& SessionID, const FString& Filename, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool UploadAnalyticsEvents(const TArray<TSharedPtr<FJsonValue>>& EventsJSON, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		bool Login(const FString& Username, const FString& Password, const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool RequestAuthTokenRefresh(const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		void Tick();

		void SetAutomationHandler(ICloudConnectionAutomation* InAutomationHandler);

		FHttpRequestRef MakeRequest(const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback, bool bRefreshTokenOnAuthFailure = true, int32* OutRequestAutomationIndexPtr = nullptr);
		static FString GetRequestTypeString(ERequestType RequestType);

	private:
		void HandleRequestResponse(const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback, bool bRefreshTokenOnAuthFailure,
			bool bSuccessfulConnection, int32 ResponseCode, const FString& ResponseContent);

		FString AuthToken;
		FString RefreshToken;
		FString XApiKey;
		ELoginStatus LoginStatus = ELoginStatus::Disconnected;
		FDateTime AuthTokenTimestamp = FDateTime(0);
		int32 NextRequestAutomationIndex = 0;
		ICloudConnectionAutomation* AutomationHandler = nullptr;
		TMap<FString, FString> CachedEncryptionKeysByToken;

		const static FTimespan AuthTokenTimeout;
};
