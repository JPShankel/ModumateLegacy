// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateCloudConnection.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Online/ModumateAccountManager.h"

#include "Serialization/JsonSerializer.h"

TAutoConsoleVariable<FString> CVarModumateCloudAddress(
	TEXT("modumate.CloudAddress"),
#if UE_BUILD_SHIPPING
	TEXT("https://account.modumate.com"),
#else
	TEXT("https://beta.account.modumate.com"),
#endif
	TEXT("The address used to connect to the Modumate Cloud backend."),
	ECVF_Default | ECVF_Cheat);


// Period for requesting refresh of AuthToken.
const FTimespan FModumateCloudConnection::AuthTokenTimeout = { 0, 10 /* min */, 0 };

FModumateCloudConnection::FModumateCloudConnection()
{
}

FString FModumateCloudConnection::GetCloudRootURL() const
{
	return CVarModumateCloudAddress.GetValueOnAnyThread();
}

FString FModumateCloudConnection::GetCloudAPIURL() const
{
	return CVarModumateCloudAddress.GetValueOnAnyThread() + TEXT("/api/v2");
}

void FModumateCloudConnection::SetLoginStatus(ELoginStatus InLoginStatus)
{
	LoginStatus = InLoginStatus;
}

ELoginStatus FModumateCloudConnection::GetLoginStatus() const
{
	return LoginStatus;
}

FString FModumateCloudConnection::GetRequestTypeString(ERequestType RequestType)
{
	switch (RequestType)
	{
		case ERequestType::Get: return TEXT("GET");
		case ERequestType::Delete: return TEXT("DELETE");
		case ERequestType::Put: return TEXT("PUT");
		case ERequestType::Post: return TEXT("POST");
	};
	return TEXT("POST");
}

bool FModumateCloudConnection::RequestEndpoint(const FString& Endpoint, ERequestType RequestType, const FRequestCustomizer& Customizer, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	if (!ensureAlways(Endpoint.Len() > 0 && Endpoint[0] == TCHAR('/')))
	{
		return false;
	}

	auto Request = MakeRequest(Callback, ServerErrorCallback);

	Request->SetURL(GetCloudAPIURL() + Endpoint);
	Request->SetVerb(GetRequestTypeString(RequestType));

	if (Customizer)
	{
		Customizer(Request);
	}

	return Request->ProcessRequest();
}

bool FModumateCloudConnection::RequestAuthTokenRefresh(const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	if (LoginStatus != ELoginStatus::Connected)
	{
		return false;
	}

	LoginStatus = ELoginStatus::WaitingForReverify;

	TWeakPtr<FModumateCloudConnection> WeakThisCaptured(AsShared());
	return RequestEndpoint(TEXT("/auth/verify"), Post,
		[WeakThisCaptured](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{
			TSharedPtr<FModumateCloudConnection> SharedThis = WeakThisCaptured.Pin();
			if (!SharedThis.IsValid())
			{
				return;
			}

			FModumateUserVerifyParams refreshParams;
			refreshParams.RefreshToken = SharedThis->RefreshToken;

			FString jsonString;
			if (FJsonObjectConverter::UStructToJsonObjectString<FModumateUserVerifyParams>(refreshParams, jsonString, 0, 0, 0, nullptr, false))
			{
				RefRequest->SetContentAsString(jsonString);
			}
		},

		[WeakThisCaptured,Callback](bool bSuccess, const TSharedPtr<FJsonObject>& Payload)
		{
			TSharedPtr<FModumateCloudConnection> SharedThis = WeakThisCaptured.Pin();
			if (!SharedThis.IsValid())
			{
				return;
			}

			FModumateUserVerifyParams verifyParams;
			FJsonObjectConverter::JsonObjectToUStruct<FModumateUserVerifyParams>(Payload.ToSharedRef(), &verifyParams);

			if (!verifyParams.AuthToken.IsEmpty())
			{
				SharedThis->AuthToken = verifyParams.AuthToken;
				SharedThis->AuthTokenTimestamp = FDateTime::Now();
				SharedThis->LoginStatus = ELoginStatus::Connected;
			}
			else
			{
				SharedThis->LoginStatus = ELoginStatus::ConnectionError;
			}

			Callback(bSuccess, Payload);
		},

		ServerErrorCallback
	);

	return false;
}

bool FModumateCloudConnection::Login(const FString& Username, const FString& Password, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	TWeakPtr<FModumateCloudConnection> WeakThisCaptured(this->AsShared());
	return RequestEndpoint(TEXT("/auth/login"), Post,
		// Customize request
		[WeakThisCaptured,Password,Username](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{
			TSharedPtr<FModumateCloudConnection> SharedThis = WeakThisCaptured.Pin();
			if (!SharedThis.IsValid())
			{
				return;
			}

			FModumateLoginParams loginParams;
			loginParams.Password = Password;
			loginParams.Username = Username;
			loginParams.AppVersion = FModumateModule::Get().GetProjectDisplayVersion();

			FString jsonString;
			if (FJsonObjectConverter::UStructToJsonObjectString<FModumateLoginParams>(loginParams, jsonString, 0, 0, 0, nullptr, false))
			{
				SharedThis->LoginStatus = ELoginStatus::WaitingForRefreshToken;
				RefRequest->SetContentAsString(jsonString);
			}
		},
			// Handle success payload
		[WeakThisCaptured,Callback,ServerErrorCallback](bool bSuccess, const TSharedPtr<FJsonObject>& Payload)
		{
			TSharedPtr<FModumateCloudConnection> SharedThis = WeakThisCaptured.Pin();
			if (!SharedThis.IsValid())
			{
				return;
			}

			FModumateUserVerifyParams verifyParams;
			FJsonObjectConverter::JsonObjectToUStruct<FModumateUserVerifyParams>(Payload.ToSharedRef(), &verifyParams);

			if (!verifyParams.RefreshToken.IsEmpty() && !verifyParams.AuthToken.IsEmpty())
			{
				SharedThis->RefreshToken = verifyParams.RefreshToken;
				SharedThis->AuthToken = verifyParams.AuthToken;
				SharedThis->LoginStatus = ELoginStatus::Connected;
			}
			else
			{
				SharedThis->LoginStatus = ELoginStatus::ConnectionError;
			}
			Callback(bSuccess, Payload);
		},
			// Handle error
		[WeakThisCaptured,ServerErrorCallback](int32 ErrorCode, const FString& ErrorString)
		{
			TSharedPtr<FModumateCloudConnection> SharedThis = WeakThisCaptured.Pin();
			if (!SharedThis.IsValid())
			{
				return;
			}
			SharedThis->LoginStatus = ELoginStatus::ConnectionError;
			ServerErrorCallback(ErrorCode, ErrorString);
		});

	return false;
}

bool FModumateCloudConnection::CreateReplay(const FString& SessionID, const FString& Version, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	auto Request = MakeRequest(Callback, ServerErrorCallback);

	Request->SetURL(GetCloudAPIURL() + TEXT("/analytics/replay/") + SessionID);
	Request->SetVerb(TEXT("PUT"));

	// TODO: refactor with Unreal JSON objects
	FString json = TEXT("{\"version\":\"") + Version + TEXT("\"}");

	Request->SetContentAsString(json);
	return Request->ProcessRequest();
}

bool FModumateCloudConnection::UploadReplay(const FString& SessionID, const FString& Filename, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	auto Request = MakeRequest(Callback,ServerErrorCallback);

	Request->SetURL(GetCloudAPIURL() + TEXT("/analytics/replay/") + SessionID + TEXT("/0"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));

	TArray<uint8> bin;
	FFileHelper::LoadFileToArray(bin, *Filename);

	Request->SetContent(bin);
	return Request->ProcessRequest();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FModumateCloudConnection::MakeRequest(const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	auto Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([Callback, ServerErrorCallback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (bWasSuccessful) 
		{
			FString content = Response->GetContentAsString();
			auto JsonResponse = TJsonReaderFactory<>::Create(content);
			TSharedPtr<FJsonValue> responseValue;
			FJsonSerializer::Deserialize(JsonResponse, responseValue);

			int32 code = Response->GetResponseCode();
			if (code == 200)
			{
				Callback(true, responseValue ? responseValue->AsObject() : nullptr);
			}
			else
			{
				ServerErrorCallback(code, responseValue && responseValue->AsObject() ? responseValue->AsObject()->GetStringField(TEXT("error")) : TEXT("null response"));
			}
		}
	});

	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	if (!AuthToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AuthToken);
	}

	return Request;
}

bool FModumateCloudConnection::UploadAnalyticsEvents(const TArray<TSharedPtr<FJsonValue>>& EventsJSON, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
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

	return RequestEndpoint(TEXT("/analytics/events/"), ERequestType::Put,
		[eventsJSONString](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{
			RefRequest->SetContentAsString(eventsJSONString);
		},
		Callback,
		ServerErrorCallback
	);
}

void FModumateCloudConnection::Tick()
{
	if (LoginStatus == ELoginStatus::Connected && FDateTime::Now() - AuthTokenTimestamp > AuthTokenTimeout)
	{
		AuthTokenTimestamp = FDateTime::Now();
		RequestAuthTokenRefresh(RefreshToken, [](bool, const TSharedPtr<FJsonObject>&) {}, [](int32, const FString&) {});
	}
}