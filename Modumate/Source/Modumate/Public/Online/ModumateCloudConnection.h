// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/ModumateSerialization.h"
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
		FString GetCloudProjectPageURL() const;
		FString GetCloudWorkspacePlansURL() const;
		FString GetInstallerDownloadURL() const;

		const FString& GetAuthToken() const { return AuthToken; }
		void SetAuthToken(const FString& InAuthToken);
		const FString& GetRefreshToken() const { return RefreshToken; }
		void SetRefreshToken(const FString& InRefreshToken);

		const FString& GetXApiKey() const { return XApiKey; }
		void SetXApiKey(const FString& InXApiKey);

		static FString MakeEncryptionToken(const FString& UserID, const FString& ProjectID);
		static bool ParseEncryptionToken(const FString& EncryptionToken, FString& OutUserID, FString& OutProjectID);

		void CacheEncryptionKey(const FString& UserID, const FString& ProjectID, const FString& EncryptionKey);
		bool ClearEncryptionKey(const FString& UserID, const FString& ProjectID);
		void ClearEncryptionKeys();
		bool GetCachedEncryptionKey(const FString& UserID, const FString& ProjectID, FString& OutEncryptionKey);
		void QueryEncryptionKey(const FString& UserID, const FString& ProjectID, const FOnEncryptionKeyResponse& Delegate);
#if !UE_BUILD_SHIPPING
		static FString MakeTestingEncryptionKey(const FString& UserID, const FString& ProjectID);
#endif

		void SetLoginStatus(ELoginStatus InLoginStatus);
		ELoginStatus GetLoginStatus() const;
		bool IsLoggedIn(bool bAllowCurrentLoggingIn = false) const;

		using FRequestCustomizer = TFunction<void(FHttpRequestRef& RefRequest)>;
		using FSuccessCallback = TFunction<void(bool,const TSharedPtr<FJsonObject>&)>;
		using FErrorCallback = TFunction<void(int32, const FString&)>;
		using FProjectCallback = TFunction<void(const FModumateDocumentHeader&, FMOIDocumentRecord&, bool)>;

		enum ERequestType { Get, Delete, Put, Post };
		bool RequestEndpoint(const FString& Endpoint, ERequestType RequestType, const FRequestCustomizer& Customizer, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback,
			bool bRefreshTokenOnAuthFailure = true, bool bSynchronous = false);

		bool CreateReplay(const FString& SessionID, const FString& Version, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool UploadReplay(const FString& SessionID, const FString& Filename, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool UploadAnalyticsEvents(const TArray<TSharedPtr<FJsonValue>>& EventsJSON, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		bool DownloadProject(const FString& ProjectID, const FProjectCallback& DownloadCallback, const FErrorCallback& ServerErrorCallback);
		bool UploadProject(const FString& ProjectID, const FModumateDocumentHeader& DocHeader, const FMOIDocumentRecord& DocRecord, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool ReadyForConnection(const FString& ProjectID);
		
		bool UploadPresetThumbnailImage(const FString& FileName, TArray<uint8>& OutData, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		bool Login(const FString& Username, const FString& Password, const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool RequestAuthTokenRefresh(const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		void OnLogout(); // This is in response to logout from ams

		void Tick();

		void SetAutomationHandler(ICloudConnectionAutomation* InAutomationHandler);

		void SetupRequestAuth(FHttpRequestRef& Request);
		FHttpRequestRef MakeRequest(const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback, bool bRefreshTokenOnAuthFailure = true, int32* OutRequestAutomationIndexPtr = nullptr);
		static FString GetRequestTypeString(ERequestType RequestType);

		static const FString EncryptionTokenKey;
		static const FString DocumentHashKey;
		static bool MakeConnectionURL(FString& OutFullURL, const FString& BaseURL, const FString& UserID, const FString& ProjectID);
		static bool ParseConnectionOptions(const FString& ConnectionOptions, FString& OutUserID, FString& OutProjectID);

		void NetworkTick(UWorld* World);

		void ReportMultiPlayerFailure(const FString& Category, const FString& Details, const FString& NetworkInfo = FString());

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
		TSet<FString> PendingProjectDownloads, PendingProjectUploads;
		double LastNetworkTickTime = 0.0;
		const static FTimespan AuthTokenTimeout;
};
