// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateCloudConnection.h"

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
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([this, Callback, ServerErrorCallback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		int32 code = Response->GetResponseCode();
		auto responseJSON = this->ParseJSONResponse(Request, Response);
		if (code == 200)
		{
			Callback(true);
		}
		else
		{
			ServerErrorCallback(code, responseJSON ? responseJSON->GetStringField(TEXT("error")) : TEXT("null response"));
		}
	});

	Request->SetURL(URL + TEXT("/auth/verify"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

	FString json = "{\"refreshToken\":\"" + InRefreshToken + "\"}";

	Request->SetContentAsString(json);
	return Request->ProcessRequest();
}

bool FModumateCloudConnection::Login(const FString& Username, const FString& Password, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([this, Callback, ServerErrorCallback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		int32 code = Response->GetResponseCode();
		auto responseJSON = this->ParseJSONResponse(Request, Response);
		if (code == 200)
		{
			Callback(true);
		} 
		else
		{
			ServerErrorCallback(code, responseJSON ? responseJSON->GetStringField(TEXT("error")) : TEXT("null response"));
		}
	});

	Request->SetURL(URL + TEXT("/auth/login"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

	FString json = "{\"username\":\"" + Username + "\", \"password\":\"" + Password + "\"}";

	Request->SetContentAsString(json);
	return Request->ProcessRequest();
}

TSharedPtr<FJsonObject> FModumateCloudConnection::ParseJSONResponse(FHttpRequestPtr Request, FHttpResponsePtr Response)
{
	FString content = Response->GetContentAsString();
	auto JsonResponse = TJsonReaderFactory<>::Create(content);
	TSharedPtr<FJsonValue> responseValue;
	if (responseValue.IsValid())
	{
		FJsonSerializer::Deserialize(JsonResponse, responseValue);
		TSharedPtr<FJsonObject> responseObject = responseValue->AsObject();

		FString refreshToken = responseObject->GetStringField(TEXT("refreshToken"));

		if (!refreshToken.IsEmpty())
		{
			RefreshToken = refreshToken;
		}

		FString authToken = responseObject->GetStringField(TEXT("authToken"));

		if (!authToken.IsEmpty())
		{
			AuthToken = authToken;
		}

		return responseObject;
	}
	return nullptr;
}


bool FModumateCloudConnection::CreateReplay(const FString& SessionID, const FString& Version, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
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
				auto responseJSON = this->ParseJSONResponse(Request, Response);
				ServerErrorCallback(code, responseJSON ? responseJSON->GetStringField(TEXT("error")) : TEXT("error - null response"));
			}
		}
	});

	Request->SetURL(URL + TEXT("/analytics/replay/") + SessionID);
	Request->SetVerb(TEXT("PUT"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AuthToken);

	FString json = "{\"version\":\"" + Version + "\"}";

	Request->SetContentAsString(json);
	return Request->ProcessRequest();
}

bool FModumateCloudConnection::UploadReplay(const FString& SessionID, const FString& Filename, const TFunction<void(bool)>& Callback, const TFunction<void(int32, FString)>& ServerErrorCallback)
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
				auto responseJSON = this->ParseJSONResponse(Request, Response);
				ServerErrorCallback(code, responseJSON ? responseJSON->GetStringField(TEXT("error")) : TEXT("null response"));
			}
		}
	});

	Request->SetURL(URL + TEXT("/analytics/replay/") + SessionID + TEXT("/0"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AuthToken);

	TArray<uint8> bin;
	FFileHelper::LoadFileToArray(bin, *Filename);

	Request->SetContent(bin);
	return Request->ProcessRequest();
}