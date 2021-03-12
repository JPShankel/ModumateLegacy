// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "JsonUtilities.h"

/**
 * Client to Cloud Connection Manager. Authentication details are passed automatically with all required requests.
 */
class MODUMATE_API FModumateCloudConnection : public TSharedFromThis<FModumateCloudConnection>
{
	public:
		FModumateCloudConnection();

		FString GetCloudRootURL() const;
		FString GetCloudAPIURL() const;

		FString GetAuthToken() const { return AuthToken; }

		void SetLoginStatus(ELoginStatus InLoginStatus);
		ELoginStatus GetLoginStatus() const;
		bool IsLoggedIn(bool bAllowCurrentLoggingIn = false) const;

		using FRequestCustomizer = TFunction<void(TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)>;
		using FSuccessCallback = TFunction<void(bool,const TSharedPtr<FJsonObject>&)>;
		using FErrorCallback = TFunction<void(int32, const FString&)>;

		enum ERequestType { Get, Delete, Put, Post };
		bool RequestEndpoint(const FString& Endpoint, ERequestType RequestType, const FRequestCustomizer& Customizer, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		bool CreateReplay(const FString& SessionID, const FString& Version, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool UploadReplay(const FString& SessionID, const FString& Filename, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool UploadAnalyticsEvents(const TArray<TSharedPtr<FJsonValue>>& EventsJSON, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		bool Login(const FString& Username, const FString& Password, const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);
		bool RequestAuthTokenRefresh(const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		void Tick();

	private:
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback);

		static FString GetRequestTypeString(ERequestType RequestType);

		FString AuthToken;
		FString RefreshToken;
		ELoginStatus LoginStatus = ELoginStatus::Disconnected;
		FDateTime AuthTokenTimestamp;
		const static FTimespan AuthTokenTimeout;
};