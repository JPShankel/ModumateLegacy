// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAccountManager.h"
#include "JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "UnrealClasses/ModumateGameInstance.h"

// Period for requesting refresh of IdToken.
const FTimespan FModumateAccountManager::IdTokenTimeout = { 0, 10 /* min */, 0 };

FString FModumateAccountManager::GetAmsAddress()
{
#if 1   // For testing against staging server:
	return TEXT("https://beta.account.modumate.com");
#else
	return TEXT("https://account.modumate.com");
#endif
}

FModumateAccountManager::FModumateAccountManager() : LoginStatus(ELoginStatus::Disconnected)
{ }

FString FModumateAccountManager::ModumateIdentityEndpoint(const FString& api)
{
	return GetAmsAddress() + TEXT("/api/v2/") + api;
}

void FModumateAccountManager::OnAmsResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		int32 code = Response->GetResponseCode();
		if (code == kSuccess)
		{
			ProcessLogin(Response);
		}
		else
		{
			if (code == kInvalidIdToken)
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

void FModumateAccountManager::OnStatusResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		int32 code = Response->GetResponseCode();
		if (code == kSuccess)
		{
			FString content = Response->GetContentAsString();
			FModumateUserStatus userStatus;
			if (FJsonObjectConverter::JsonObjectStringToUStruct<FModumateUserStatus>(content, &userStatus, 0, 0))
			{
				if (!userStatus.Active)
				{
					LoginStatus = ELoginStatus::UserDisabled;
				}
				ProcessUserStatus(userStatus);
			}
		}
		else
		{
			LoginStatus = ELoginStatus::Disconnected;
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
		{this->OnAmsResponseReceived(Request, Response, bWasSuccessful); });
	FModumateLoginParams loginParams;
	loginParams.Username = userName;
	loginParams.Password = password;

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
		auto JsonResponse = TJsonReaderFactory<>::Create(content);
		TSharedPtr<FJsonValue> responseValue;
		FJsonSerializer::Deserialize(JsonResponse, responseValue);
		auto responseObject = responseValue->AsObject();
		if (responseObject)
		{
			FString refreshToken = responseObject->GetStringField(TEXT("refreshToken"));
			if (!refreshToken.IsEmpty())
			{
				RefreshToken = refreshToken;
				LoginStatus = ELoginStatus::HaveValidRefreshToken;
				ProcessLogin(nullptr);
			}
		}
		else
		{
			LoginStatus = ELoginStatus::UnknownError;
		}
		break;
	}

	case ELoginStatus::HaveValidRefreshToken:
	{
		RequestIdTokenRefresh();
		break;
	}

	case ELoginStatus::WaitingForVerify:
	{
		FString content = Response->GetContentAsString();
		FModumateUserVerifyParams userVerifyInfo;
		FJsonObjectConverter::JsonObjectStringToUStruct<FModumateUserVerifyParams>(content, &userVerifyInfo, 0, 0);
		if (!userVerifyInfo.IdToken.IsEmpty())
		{
			LocalId = userVerifyInfo.User.Uid;
			RefreshToken = userVerifyInfo.RefreshToken;
			IdToken = userVerifyInfo.IdToken;
			UserInfo = userVerifyInfo.User;
			IdTokenTimestamp = FDateTime::Now();
			LoginStatus = ELoginStatus::Connected;
			ProcessUserStatus(userVerifyInfo.Status);
		}
		else
		{
			LoginStatus = ELoginStatus::UnknownError;
		}

		for (auto& callback: TokenRefreshDelegates)
		{
			callback.Execute(LoginStatus == ELoginStatus::Connected);
		}
		TokenRefreshDelegates.Empty();
		break;
	}

	default:
		break;
	}
}

void FModumateAccountManager::RequestIdTokenRefresh(TBaseDelegate<void, bool>* callback)
{
	if (LoginStatus != ELoginStatus::HaveValidRefreshToken && LoginStatus != ELoginStatus::Connected)
	{
		return;
	}

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{this->OnAmsResponseReceived(Request, Response, bWasSuccessful); });

	Request->SetURL(ModumateIdentityEndpoint(TEXT("auth/verify")));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

	TSharedPtr<FJsonObject> tokenRequestJson = MakeShared<FJsonObject>();
	tokenRequestJson->SetStringField(TEXT("refreshToken"), RefreshToken);
	FJsonDataBag serialize;
	serialize.JsonObject = tokenRequestJson;
	FString payload = serialize.ToJson(false);

	if (callback != nullptr)
	{
		TokenRefreshDelegates.Add(*callback);
	}
	Request->SetContentAsString(payload);
	LoginStatus = ELoginStatus::WaitingForVerify;
	Request->ProcessRequest();
}

void FModumateAccountManager::RequestStatus()
{
	if (LoginStatus != ELoginStatus::Connected)
	{
		return;
	}

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{this->OnStatusResponseReceived(Request, Response, bWasSuccessful); });

	Request->SetURL(ModumateIdentityEndpoint(TEXT("status")));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + GetIdToken());

	Request->ProcessRequest();
}

bool FModumateAccountManager::HasPermission(EModumatePermission requestedPermission) const
{
	return CurrentPermissions.Contains(requestedPermission);
}

void FModumateAccountManager::ProcessUserStatus(const FModumateUserStatus& userStatus)
{
	// TODO: process announcements:
	CurrentPermissions.Empty();

	for (const FString& perm: userStatus.Permissions)
	{
		auto permissionIndex = StaticEnum<EModumatePermission>()->GetValueByNameString(perm);
		if (permissionIndex != INDEX_NONE)
		{
			CurrentPermissions.Add(EModumatePermission(permissionIndex));
		}
	}
}

void FModumateAccountManager::Tick()
{
	if (LoginStatus == ELoginStatus::Connected && FDateTime::Now() - IdTokenTimestamp > IdTokenTimeout)
	{
		IdTokenTimestamp = FDateTime::Now();
		RequestStatus();
	}
}
