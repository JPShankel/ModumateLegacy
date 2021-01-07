// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "JsonUtilities.h"

/**
 * Client to Cloud Connection Manager. Authentication details are passed automatically with all required requests.
 */
class MODUMATE_API FModumateCloudConnection
{
	public:
		void SetUrl(const FString& InURL);
		void SetAuthToken(const FString& InAuthToken);

		bool Login(const FString& InRefreshToken, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback);
		bool Login(const FString& Username, const FString& Password, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback);

		bool CreateReplay(const FString& SessionID, const FString& Version, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback);
		bool UploadReplay(const FString& SessionID, const FString& Filename, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback);
		bool UploadSessionTime(const FTimespan& SessionDuration, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback);

	private:
		TSharedPtr<FJsonObject> ParseJSONResponse(FHttpRequestPtr Request, FHttpResponsePtr Response);

		TSharedRef<IHttpRequest> MakeRequest(const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback);
		bool SendAnalyticsEvent(const FString& Key, float Value, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback);

		FString AuthToken;
		FString RefreshToken;
		FString URL;
};