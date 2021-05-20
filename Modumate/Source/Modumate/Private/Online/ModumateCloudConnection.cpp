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
const FTimespan FModumateCloudConnection::AuthTokenTimeout = { 0, 5 /* min */, 0 };

FModumateCloudConnection::FModumateCloudConnection()
{
	AuthTokenTimestamp = FDateTime::Now();
}

FString FModumateCloudConnection::GetCloudRootURL() const
{
	return CVarModumateCloudAddress.GetValueOnAnyThread();
}

FString FModumateCloudConnection::GetCloudAPIURL() const
{
	return CVarModumateCloudAddress.GetValueOnAnyThread() + TEXT("/api/v2");
}

void FModumateCloudConnection::SetAuthToken(const FString& InAuthToken)
{
	AuthToken = InAuthToken;
}

FString FModumateCloudConnection::MakeEncryptionToken(const FString& UserID, const FGuid& SessionID)
{
	return UserID + SUBOBJECT_DELIMITER + SessionID.ToString(EGuidFormats::Short);
}

bool FModumateCloudConnection::ParseEncryptionToken(const FString& EncryptionToken, FString& OutUserID, FGuid& OutSessionID)
{
	OutUserID.Empty();
	OutSessionID.Invalidate();

	FString sessionIDStr;
	return EncryptionToken.Split(SUBOBJECT_DELIMITER, &OutUserID, &sessionIDStr) &&
		!OutUserID.IsEmpty() && FGuid::Parse(sessionIDStr, OutSessionID);
}

bool FModumateCloudConnection::GetCachedEncryptionKey(const FString& UserID, const FGuid& SessionID, FString& OutEncryptionKey)
{
	FString encryptionToken = MakeEncryptionToken(UserID, SessionID);

	if (CachedEncryptionKeysByToken.Contains(encryptionToken))
	{
		OutEncryptionKey = CachedEncryptionKeysByToken[encryptionToken];
		return true;
	}

	return false;
}

void FModumateCloudConnection::QueryEncryptionKey(const FString& UserID, const FGuid& SessionID, const FOnEncryptionKeyResponse& Delegate)
{
	FString encryptionKeyString;
	if (!GetCachedEncryptionKey(UserID, SessionID, encryptionKeyString))
	{
		FString encryptionToken = MakeEncryptionToken(UserID, SessionID);

		// TODO: asynchronously retrieve a real encryption key from the AMS using the User ID & Session ID, and *then* call the delegate.
		encryptionKeyString = UserID.Reverse() + SessionID.ToString(EGuidFormats::Short).Reverse();
		CachedEncryptionKeysByToken.Add(encryptionToken, encryptionKeyString);
	}

	FEncryptionKeyResponse response(EEncryptionResponse::Success, TEXT(""));
	response.EncryptionData.Key.Append((uint8*)*encryptionKeyString, sizeof(TCHAR) * encryptionKeyString.Len());
	Delegate.ExecuteIfBound(response);
}

void FModumateCloudConnection::SetLoginStatus(ELoginStatus InLoginStatus)
{
	LoginStatus = InLoginStatus;
}

ELoginStatus FModumateCloudConnection::GetLoginStatus() const
{
	return LoginStatus;
}

bool FModumateCloudConnection::IsLoggedIn(bool bAllowCurrentLoggingIn) const
{
	switch (LoginStatus)
	{
	case ELoginStatus::Connected:
	case ELoginStatus::WaitingForReverify:
		return true;
	case ELoginStatus::WaitingForRefreshToken:
		return bAllowCurrentLoggingIn;
	default:
		return false;
	}
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

bool FModumateCloudConnection::RequestEndpoint(const FString& Endpoint, ERequestType RequestType, const FRequestCustomizer& Customizer, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback,
	bool bRefreshTokenOnAuthFailure)
{
	if (!ensureAlways(Endpoint.Len() > 0 && Endpoint[0] == TCHAR('/')))
	{
		return false;
	}

	int32 requestAutomationIndex = INDEX_NONE;
	auto Request = MakeRequest(Callback, ServerErrorCallback, bRefreshTokenOnAuthFailure, &requestAutomationIndex);

	Request->SetURL(GetCloudAPIURL() + Endpoint);
	Request->SetVerb(GetRequestTypeString(RequestType));

	if (Customizer)
	{
		Customizer(Request);
	}

	// Allow an automation handler to log the request, and potentially simulate the stored response to the request.
	if (AutomationHandler)
	{
		AutomationHandler->RecordRequest(Request, requestAutomationIndex);

		// If we're playing back the response as it was logged, then set a timer to handle the response as if we actually received it.
		float responseTime;
		bool bAutomatedSuccess;
		int32 automatedCode;
		FString automatedContent;
		if (AutomationHandler->GetResponse(Request, requestAutomationIndex, bAutomatedSuccess, automatedCode, automatedContent, responseTime))
		{
			TWeakPtr<FModumateCloudConnection> weakThisCaptured(this->AsShared());
			FTimerManager& timerManager = AutomationHandler->GetTimerManager();
			FTimerHandle responseHandlerTimer;

			timerManager.SetTimer(responseHandlerTimer, [weakThisCaptured, Callback, ServerErrorCallback, bRefreshTokenOnAuthFailure, requestAutomationIndex, bAutomatedSuccess, automatedCode, automatedContent]() {
				TSharedPtr<FModumateCloudConnection> sharedThis = weakThisCaptured.Pin();
				if (sharedThis.IsValid())
				{
					sharedThis->HandleRequestResponse(Callback, ServerErrorCallback, bRefreshTokenOnAuthFailure, bAutomatedSuccess, automatedCode, automatedContent);
				}
				}, responseTime, false);

			return true;
		}
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

		ServerErrorCallback,
		false
	);

	return false;
}

bool FModumateCloudConnection::Login(const FString& Username, const FString& Password, const FString& InRefreshToken, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	TWeakPtr<FModumateCloudConnection> WeakThisCaptured(this->AsShared());
	return RequestEndpoint(TEXT("/auth/login"), Post,
		// Customize request
		[WeakThisCaptured,Password,Username,InRefreshToken](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{
			TSharedPtr<FModumateCloudConnection> SharedThis = WeakThisCaptured.Pin();
			if (!SharedThis.IsValid())
			{
				return;
			}

			FModumateLoginParams loginParams;
			loginParams.Username = Username;
			loginParams.Password = Password;
			loginParams.RefreshToken = InRefreshToken;
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
				SharedThis->AuthTokenTimestamp = FDateTime::Now();
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
			bool bInvalidCredentials = (ErrorCode == EHttpResponseCodes::Denied);
			SharedThis->LoginStatus = bInvalidCredentials ? ELoginStatus::InvalidCredentials : ELoginStatus::ConnectionError;
			ServerErrorCallback(ErrorCode, ErrorString);
		},
		false);

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

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FModumateCloudConnection::MakeRequest(const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback,
	bool bRefreshTokenOnAuthFailure, int32* OutRequestAutomationIndexPtr)
{
	int32 curRequestAutomationIdx = INDEX_NONE;
	if (OutRequestAutomationIndexPtr)
	{
		curRequestAutomationIdx = NextRequestAutomationIndex++;
		*OutRequestAutomationIndexPtr = curRequestAutomationIdx;
	}

	TWeakPtr<FModumateCloudConnection> weakThisCaptured(AsShared());
	auto Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([Callback, ServerErrorCallback, weakThisCaptured, bRefreshTokenOnAuthFailure, curRequestAutomationIdx]
	(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		int32 code = Response->GetResponseCode();
		FString content = Response->GetContentAsString();
		TSharedPtr<FModumateCloudConnection> sharedThis = weakThisCaptured.Pin();

		if (sharedThis.IsValid())
		{
			// If there's an automation handler, then store the response we get for this request so we can potentially play it back.
			if (sharedThis->AutomationHandler && Request.IsValid() && (curRequestAutomationIdx != INDEX_NONE))
			{
				sharedThis->AutomationHandler->RecordResponse(Request.ToSharedRef(), curRequestAutomationIdx, bWasSuccessful, code, content);
			}

			sharedThis->HandleRequestResponse(Callback, ServerErrorCallback, bRefreshTokenOnAuthFailure, bWasSuccessful, code, content);
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

void FModumateCloudConnection::HandleRequestResponse(const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback, bool bRefreshTokenOnAuthFailure,
	bool bSuccessfulConnection, int32 ResponseCode, const FString& ResponseContent)
{
	auto JsonResponse = TJsonReaderFactory<>::Create(ResponseContent);
	TSharedPtr<FJsonValue> responseValue;
	FJsonSerializer::Deserialize(JsonResponse, responseValue);

	if (ResponseCode == EHttpResponseCodes::Ok)
	{
		Callback(true, responseValue ? responseValue->AsObject() : nullptr);
	}
	else
	{
		// If any request fails due to bad auth, then reset the auth token timestamp so that we re-verify the next chance we get.
		// TODO: For many requests, it might make more sense to immediately refresh the token from within this function,
		// and re-try the original request so the callback has a chance to come back successfully.
		if ((ResponseCode == EHttpResponseCodes::Denied) && bRefreshTokenOnAuthFailure)
		{
			AuthTokenTimestamp = FDateTime(0);
		}

		ServerErrorCallback(ResponseCode, responseValue && responseValue->AsObject() ? responseValue->AsObject()->GetStringField(TEXT("error")) : TEXT("null response"));
	}
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
	FTimespan timeSinceLastAuth = (FDateTime::Now() - AuthTokenTimestamp);
	if ((LoginStatus == ELoginStatus::Connected) && (timeSinceLastAuth > AuthTokenTimeout))
	{
		AuthTokenTimestamp = FDateTime::Now();
		RequestAuthTokenRefresh(RefreshToken, [](bool, const TSharedPtr<FJsonObject>&) {}, [](int32, const FString&) {});
	}
}

void FModumateCloudConnection::SetAutomationHandler(ICloudConnectionAutomation* InAutomationHandler)
{
	AutomationHandler = InAutomationHandler;
	NextRequestAutomationIndex = 0;
}
