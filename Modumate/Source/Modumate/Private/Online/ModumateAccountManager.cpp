// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAccountManager.h"
#include "JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "UnrealClasses/ModumateGameInstance.h"

namespace Modumate
{
	const FString FModumateAccountManager::ModumateIdentityURL
		{ TEXT("https://account.modumate.com/api/v1/") };

	const FString FModumateAccountManager::ApiKey
		{ TEXT("384hj4592323f2434242354t5") };

	FModumateAccountManager::FModumateAccountManager() : LoginStatus(ELoginStatus::Disconnected)
	{ }

	FString FModumateAccountManager::ModumateIdentityEndpoint(const FString& api)
	{
		return ModumateIdentityURL + api;
	}

	void FModumateAccountManager::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (bWasSuccessful)
		{
			int32 code = Response->GetResponseCode();
			if (code == 200)
			{
				ProcessLogin(Response);
			}
			else
			{
				FString content = Response->GetContentAsString();

				if (content == TEXT("Unauthorized"))
				{
					LoginStatus = ELoginStatus::InvalidPassword;
				}
				else
				{
					LoginStatus = ELoginStatus::UnknownError;
				}
			}
		}
		else
		{
			LoginStatus = ELoginStatus::ConnectionError;
		}
	}


	void FModumateAccountManager::Login(const FString& userName, const FString& password)
	{
		TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
		Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
			{this->OnLoginResponseReceived(Request, Response, bWasSuccessful); });
		FModumateLoginParams loginParams;
		loginParams.key = ApiKey;
		loginParams.username = userName;
		loginParams.password = password;

		Request->SetURL(ModumateIdentityEndpoint(TEXT("auth/login")) );
		Request->SetVerb(TEXT("POST"));
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
		Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

		FString payload;
		FJsonObjectConverter::UStructToJsonObjectString<FModumateLoginParams>(loginParams, payload,
			0, 0, 0, nullptr, false);
		Request->SetContentAsString(payload);
		LoginStatus = ELoginStatus::WaitingForRefreshToken;
		Request->ProcessRequest();
	}

	void FModumateAccountManager::ProcessLogin(const FHttpResponsePtr Response)
	{
		switch (LoginStatus)
		{
		case ELoginStatus::WaitingForRefreshToken:
		{
			FString content = Response->GetContentAsString();

			FModumateLoginResponse loginResponse;
			FJsonObjectConverter::JsonObjectStringToUStruct<FModumateLoginResponse>(content, &loginResponse, 0, 0);
			if (!loginResponse.refreshToken.IsEmpty())
			{
				RefreshToken = loginResponse.refreshToken;
				LoginStatus = ELoginStatus::ValidRefreshToken;
				ProcessLogin(nullptr);
			}
			else
			{
				LoginStatus = ELoginStatus::UnknownError;
			}
			break;
		}

		case ELoginStatus::ValidRefreshToken:
		{
			TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
			Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
				{this->OnLoginResponseReceived(Request, Response, bWasSuccessful); });

			Request->SetURL(ModumateIdentityEndpoint(TEXT("auth/verify")) );
			Request->SetVerb(TEXT("POST"));
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
			Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

			TSharedPtr<FJsonObject> tokenRequestJson = MakeShared<FJsonObject>();
			tokenRequestJson->SetStringField(TEXT("key"), ApiKey);
			tokenRequestJson->SetStringField(TEXT("refreshToken"), RefreshToken);
			FJsonDataBag serialize;
			serialize.JsonObject = tokenRequestJson;
			FString payload = serialize.ToJson(false);

			Request->SetContentAsString(payload);
			LoginStatus = ELoginStatus::WaitingForAuthId;
			Request->ProcessRequest();
			break;
		}

		case ELoginStatus::WaitingForAuthId:
		{
			FString content = Response->GetContentAsString();
			FModumateTokenInfo tokenInfo;
			FJsonObjectConverter::JsonObjectStringToUStruct<FModumateTokenInfo>(content, &tokenInfo, 0, 0);
			if (!tokenInfo.idToken.IsEmpty())
			{
				LocalId = tokenInfo.user.uid;
				RefreshToken = tokenInfo.refreshToken;
				IdToken = tokenInfo.idToken;
				UserInfo = tokenInfo.user;
				LoginStatus = ELoginStatus::Connected;
			}
			else
			{
				LoginStatus = ELoginStatus::UnknownError;
			}
			break;
		}

		default:
			break;
		}
	}
}