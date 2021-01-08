// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateCloudConnection.h"

#include "Serialization/JsonSerializer.h"

void FModumateCloudConnection::SetUrl(const FString& InURL)
{
	URL = InURL + "/api/v2";
}

void FModumateCloudConnection::SetAuthToken(const FString& InAuthToken)
{
	AuthToken = InAuthToken;
}

bool FModumateCloudConnection::Login(const FString& InRefreshToken, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
{
	TSharedRef<IHttpRequest> Request = MakeRequest(Callback, ServerErrorCallback);

	Request->SetURL(URL + TEXT("/auth/verify"));
	Request->SetVerb(TEXT("POST"));

	// TODO: refactor with Unreal JSON objects
	FString json = "{\"refreshToken\":\"" + InRefreshToken + "\"}";

	Request->SetContentAsString(json);
	return Request->ProcessRequest();
}

bool FModumateCloudConnection::Login(const FString& Username, const FString& Password, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
{
	TSharedRef<IHttpRequest> Request = MakeRequest(Callback, ServerErrorCallback);

	Request->SetURL(URL + TEXT("/auth/login"));
	Request->SetVerb(TEXT("POST"));

	// TODO: refactor with Unreal JSON objects
	FString json = "{\"username\":\"" + Username + "\", \"password\":\"" + Password + "\"}";

	Request->SetContentAsString(json);
	return Request->ProcessRequest();
}

bool FModumateCloudConnection::CreateReplay(const FString& SessionID, const FString& Version, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
{
	TSharedRef<IHttpRequest> Request = MakeRequest(Callback, ServerErrorCallback);

	Request->SetURL(URL + TEXT("/analytics/replay/") + SessionID);
	Request->SetVerb(TEXT("PUT"));

	// TODO: refactor with Unreal JSON objects
	FString json = "{\"version\":\"" + Version + "\"}";

	Request->SetContentAsString(json);
	return Request->ProcessRequest();
}

bool FModumateCloudConnection::UploadReplay(const FString& SessionID, const FString& Filename, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
{
	TSharedRef<IHttpRequest> Request = MakeRequest(Callback,ServerErrorCallback);

	Request->SetURL(URL + TEXT("/analytics/replay/") + SessionID + TEXT("/0"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));

	TArray<uint8> bin;
	FFileHelper::LoadFileToArray(bin, *Filename);

	Request->SetContent(bin);
	return Request->ProcessRequest();
}

TSharedPtr<FJsonObject> FModumateCloudConnection::ParseJSONResponse(FHttpRequestPtr Request, FHttpResponsePtr Response)
{
	FString content = Response->GetContentAsString();
	auto JsonResponse = TJsonReaderFactory<>::Create(content);
	TSharedPtr<FJsonValue> responseValue;
	FJsonSerializer::Deserialize(JsonResponse, responseValue);
	if (responseValue.IsValid())
	{
		TSharedPtr<FJsonObject> responseObject = responseValue->AsObject();

		FString newRefreshToken;
		if (responseObject->TryGetStringField(TEXT("refreshToken"), newRefreshToken) && !newRefreshToken.IsEmpty())
		{
			RefreshToken = newRefreshToken;
		}

		FString newAuthToken;
		if (responseObject->TryGetStringField(TEXT("authToken"), newAuthToken) && !newAuthToken.IsEmpty())
		{
			AuthToken = newAuthToken;
		}

		return responseObject;
	}
	return nullptr;
}

TSharedRef<IHttpRequest> FModumateCloudConnection::MakeRequest(const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([this, Callback, ServerErrorCallback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (bWasSuccessful) {
			int32 code = Response->GetResponseCode();
			if (code == 200) {
				Callback(true);
			}
			else
			{
				auto responseJSON = ParseJSONResponse(Request, Response);
				ServerErrorCallback(code, responseJSON ? responseJSON->GetStringField(TEXT("error")) : TEXT("null response"));
			}
		}
	});

	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AuthToken);
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	return Request;
}

bool FModumateCloudConnection::UploadAnalyticsEvents(const TArray<TSharedPtr<FJsonValue>>& EventsJSON, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
{
	if (EventsJSON.Num() == 0)
	{
		return false;
	}

	FString eventsJSONString;
	auto jsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&eventsJSONString, 0);
	if (!FJsonSerializer::Serialize(EventsJSON, jsonWriter, true))
	{
		return false;
	}

	TSharedRef<IHttpRequest> Request = MakeRequest(Callback, ServerErrorCallback);

	Request->SetURL(URL + TEXT("/analytics/events/"));
	Request->SetVerb(TEXT("PUT"));
	Request->SetContentAsString(eventsJSONString);

	return Request->ProcessRequest();
}
