// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateCloudConnection.h"

#include "HttpManager.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/PlatformFunctions.h"
#include "ModumateCore/PrettyJSONWriter.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ProjectConnection.h"
#include "HAL/Event.h"
#include "UnrealClasses/DynamicIconGenerator.h"

/*
* With this (without the ECVF_Cheat flag) we can override the AMS endpoint on any shipping build (installer or otherwise) by adding these lines to:
* %LOCALAPPDATA%\Modumate\Saved\Config\WindowsNoEditor\Engine.ini:
* 
* [SystemSettings]
* modumate.CloudAddress="https://beta.account.modumate.com"
*/
TAutoConsoleVariable<FString> CVarModumateCloudAddress(
	TEXT("modumate.CloudAddress"),
#if UE_BUILD_SHIPPING
	TEXT("https://account.modumate.com"),
#else
	TEXT("https://beta.account.modumate.com"),
#endif
	TEXT("The address used to connect to the Modumate Cloud backend."),
	ECVF_Default);


// Period for requesting refresh of AuthToken.
const FTimespan FModumateCloudConnection::AuthTokenTimeout = { 0, 5 /* min */, 0 };
const FString FModumateCloudConnection::EncryptionTokenKey(TEXT("EncryptionToken"));
const FString FModumateCloudConnection::DocumentHashKey(TEXT("DocHash"));

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
	return GetCloudRootURL() + TEXT("/api/v2");
}

FString FModumateCloudConnection::GetCloudProjectPageURL() const
{
	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	FString currentVersion = projectSettings->ProjectVersion;
	return GetCloudRootURL() + TEXT("/workspace/projects?version=") + currentVersion;
}

FString FModumateCloudConnection::GetCloudWorkspacePlansURL() const
{
	return GetCloudRootURL() + TEXT("/workspace/plans");
}

FString FModumateCloudConnection::GetInstallerDownloadURL() const
{
#if PLATFORM_MAC
	static const FString downloadLocation(TEXT("/static/download/appleintel"));
#else
	static const FString downloadLocation(TEXT("/static/download/windows64"));
#endif
	return GetCloudRootURL() + downloadLocation;
}

void FModumateCloudConnection::SetAuthToken(const FString& InAuthToken)
{
	AuthToken = InAuthToken;
}

void FModumateCloudConnection::SetRefreshToken(const FString& InRefreshToken)
{
	RefreshToken = InRefreshToken;
}

void FModumateCloudConnection::SetXApiKey(const FString& InXApiKey)
{
	XApiKey = InXApiKey;
}

FString FModumateCloudConnection::MakeEncryptionToken(const FString& UserID, const FString& ProjectID)
{
	return UserID + SUBOBJECT_DELIMITER + ProjectID;
}

bool FModumateCloudConnection::ParseEncryptionToken(const FString& EncryptionToken, FString& OutUserID, FString& OutProjectID)
{
	OutUserID.Empty();
	OutProjectID.Empty();

	return EncryptionToken.Split(SUBOBJECT_DELIMITER, &OutUserID, &OutProjectID) && !OutUserID.IsEmpty() && !OutProjectID.IsEmpty();
}

void FModumateCloudConnection::CacheEncryptionKey(const FString& UserID, const FString& ProjectID, const FString& EncryptionKey)
{
	FString encryptionToken = MakeEncryptionToken(UserID, ProjectID);
	CachedEncryptionKeysByToken.Add(encryptionToken, EncryptionKey);
}

bool FModumateCloudConnection::ClearEncryptionKey(const FString& UserID, const FString& ProjectID)
{
	FString encryptionToken = MakeEncryptionToken(UserID, ProjectID);
	return CachedEncryptionKeysByToken.Remove(encryptionToken) > 0;
}

void FModumateCloudConnection::ClearEncryptionKeys()
{
	CachedEncryptionKeysByToken.Reset();
}

bool FModumateCloudConnection::GetCachedEncryptionKey(const FString& UserID, const FString& ProjectID, FString& OutEncryptionKey)
{
	FString encryptionToken = MakeEncryptionToken(UserID, ProjectID);

	if (CachedEncryptionKeysByToken.Contains(encryptionToken))
	{
		OutEncryptionKey = CachedEncryptionKeysByToken[encryptionToken];
		return true;
	}

	return false;
}

void FModumateCloudConnection::QueryEncryptionKey(const FString& UserID, const FString& ProjectID, const FOnEncryptionKeyResponse& Delegate)
{
	FString encryptionKeyString;

	if (!GetCachedEncryptionKey(UserID, ProjectID, encryptionKeyString))
	{
		// Only the standalone server should be trying (and able) to query the AMS for an arbitrary user's encryption key.
#if UE_SERVER
		TWeakPtr<FModumateCloudConnection> weakThisCaptured(this->AsShared());
		FString getConnectionEndpoint = FProjectConnectionHelpers::MakeProjectConnectionEndpoint(ProjectID) / UserID;
		bool bQuerySuccess = RequestEndpoint(getConnectionEndpoint, ERequestType::Get, nullptr,
			[weakThisCaptured, UserID, ProjectID, Delegate](bool bSuccess, const TSharedPtr<FJsonObject>& Payload)
			{
				FProjectConnectionResponse getConnectionResponse;
				TSharedPtr<FModumateCloudConnection> sharedThis = weakThisCaptured.Pin();
				if (!ensure(bSuccess && sharedThis.IsValid() && Payload.IsValid() &&
					FJsonObjectConverter::JsonObjectToUStruct(Payload.ToSharedRef(), &getConnectionResponse)))
				{
					return;
				}
				FEncryptionKeyResponse asyncResponse(EEncryptionResponse::Success, TEXT(""));

	#if UE_BUILD_SHIPPING
				sharedThis->CacheEncryptionKey(UserID, ProjectID, getConnectionResponse.Key);
				asyncResponse.EncryptionData.Key.Append((uint8*)*getConnectionResponse.Key, sizeof(TCHAR) * getConnectionResponse.Key.Len());
	#else
				//This allows local servers to query the connection and project tables if they log a second client
				// in to the *real* cloud server project. Added this because testing responses after deploying to staging/beta
				// was taking too long.
				if (sharedThis.IsValid())
				{
					FString fakeEncryptionKey = MakeTestingEncryptionKey(UserID, ProjectID);
					sharedThis->CacheEncryptionKey(UserID, ProjectID, fakeEncryptionKey);

					asyncResponse.Response = EEncryptionResponse::Success;
					asyncResponse.EncryptionData.Key.Append((uint8*)*fakeEncryptionKey, sizeof(TCHAR) * fakeEncryptionKey.Len());
				}
	#endif

				Delegate.ExecuteIfBound(asyncResponse);

			},
			[weakThisCaptured, UserID, ProjectID, Delegate](int32 ErrorCode, const FString& ErrorString)
			{
				FEncryptionKeyResponse asyncResponse(EEncryptionResponse::Failure, ErrorString);

	#if UE_BUILD_SHIPPING || PLATFORM_LINUX
				// Shipping server configurations that failed to query for an encryption key should prevent the user from logging in.
				UE_LOG(LogTemp, Error, TEXT("Error code %d querying encrypion key for User ID %s and Project ID %s: %s"),
					ErrorCode, *UserID, *ProjectID, *ErrorString);
	#else
				// But development servers can fall back to a fake key to let development clients in.
				TSharedPtr<FModumateCloudConnection> sharedThis = weakThisCaptured.Pin();
				if (sharedThis.IsValid())
				{
					FString fakeEncryptionKey = MakeTestingEncryptionKey(UserID, ProjectID);
					sharedThis->CacheEncryptionKey(UserID, ProjectID, fakeEncryptionKey);
					UE_LOG(LogTemp, Warning, TEXT("Error code %d querying encrypion key for User ID %s and Project ID %s: %s - using a fake one for testing: %s"),
						ErrorCode, *UserID, *ProjectID, *ErrorString, *fakeEncryptionKey);

					asyncResponse.Response = EEncryptionResponse::Success;
					asyncResponse.EncryptionData.Key.Append((uint8*)*fakeEncryptionKey, sizeof(TCHAR) * fakeEncryptionKey.Len());
				}
	#endif

				Delegate.ExecuteIfBound(asyncResponse);
			},
			false);

		// If we're querying for the user's encryption key now, then exit early and respond to the delegate once the request finishes.
		if (bQuerySuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("Querying for User ID %s's encryption key for Project ID %s..."), *UserID, *ProjectID);
			return;
		}
#else
		// Otherwise, standalone clients shouldn't have reached this point; they should have gotten an encryption key when creating a connection from the AMS.
		UE_LOG(LogTemp, Error, TEXT("Client didn't already have a cached encryption key for User ID: %s!"), *UserID);
#endif
	}

	// If we haven't exited early due to waiting for an AMS query, then we can respond now with success/failure based on the cached/generated key (or lack thereof).
	if (encryptionKeyString.IsEmpty())
	{
		FEncryptionKeyResponse response(EEncryptionResponse::Failure, TEXT("Failed to query for encryption key!"));
		Delegate.ExecuteIfBound(response);
	}
	else
	{
		FEncryptionKeyResponse response(EEncryptionResponse::Success, TEXT(""));
		response.EncryptionData.Key.Append((uint8*)*encryptionKeyString, sizeof(TCHAR) * encryptionKeyString.Len());
		Delegate.ExecuteIfBound(response);
	}
	
}

#if !UE_BUILD_SHIPPING
FString FModumateCloudConnection::MakeTestingEncryptionKey(const FString& UserID, const FString& ProjectID)
{
	return FString::Printf(TEXT("TEST:%s:KEY"), *MakeEncryptionToken(UserID, ProjectID));
}
#endif

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

bool FModumateCloudConnection::MakeConnectionURL(FString& OutFullURL, const FString& BaseURL, const FString& UserID, const FString& ProjectID)
{
	OutFullURL.Empty();
	if (BaseURL.IsEmpty() || UserID.IsEmpty() || ProjectID.IsEmpty())
	{
		return false;
	}

	OutFullURL = FString::Printf(TEXT("%s?%s=%s"), *BaseURL, *EncryptionTokenKey, *MakeEncryptionToken(UserID, ProjectID));

	return true;
}

bool FModumateCloudConnection::ParseConnectionOptions(const FString& ConnectionOptions, FString& OutUserID, FString& OutProjectID)
{
	FString encryptionToken = UGameplayStatics::ParseOption(ConnectionOptions, EncryptionTokenKey);
	return ParseEncryptionToken(encryptionToken, OutUserID, OutProjectID);
}

void FModumateCloudConnection::NetworkTick(UWorld* World)
{
	static constexpr float checkinMaxDelay = 0.1f;

	double currentTime = FPlatformTime::Seconds();
	float deltaTime = float(currentTime - LastNetworkTickTime);
	if (deltaTime >= checkinMaxDelay)
	{
		UNetDriver* netDriver = GEngine->FindNamedNetDriver(World, NAME_GameNetDriver);
		if (netDriver)
		{
			netDriver->TickDispatch(deltaTime);
			netDriver->TickFlush(deltaTime);
			LastNetworkTickTime = currentTime;
		}

	}
}

bool FModumateCloudConnection::RequestEndpoint(const FString& Endpoint, ERequestType RequestType,
	const FRequestCustomizer& Customizer, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback,
	bool bRefreshTokenOnAuthFailure, bool bSynchronous)
{
	bool bRequestFinished = false;
	
	if (!ensureAlways(Endpoint.Len() > 0 && Endpoint[0] == TCHAR('/')))
	{
		return false;
	}

	auto myCallback = Callback;
	auto myError = ServerErrorCallback;

	if(bSynchronous)
	{
		myCallback = [&](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			Callback(bSuccessful, Response);
			bRequestFinished = true;
		};

		myError = [&](int32 ErrorCode, const FString& ErrorString)
		{
			ServerErrorCallback(ErrorCode, ErrorString);
			bRequestFinished = true;
		};	
	}

	
	int32 requestAutomationIndex = INDEX_NONE;
	auto Request = MakeRequest(myCallback, myError, bRefreshTokenOnAuthFailure, &requestAutomationIndex);

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

			timerManager.SetTimer(responseHandlerTimer, [weakThisCaptured, myCallback, myError, bRefreshTokenOnAuthFailure, requestAutomationIndex, bAutomatedSuccess, automatedCode, automatedContent]() {
				TSharedPtr<FModumateCloudConnection> sharedThis = weakThisCaptured.Pin();
				if (sharedThis.IsValid())
				{
					sharedThis->HandleRequestResponse(myCallback, myError, bRefreshTokenOnAuthFailure, bAutomatedSuccess, automatedCode, automatedContent);
				}
				}, responseTime, false);

			return true;
		}
	}

	bool rtn = Request->ProcessRequest();
	if(bSynchronous)
	{
		//Block until this particular request has been completed
		//NOTE that other requests may complete during this time and
		// their handlers could be called.
		double BeginWaitTime = FPlatformTime::Seconds();
		double LastTime = BeginWaitTime;
		double CurrentWaitTime = 0;
		//PRESET_INTEGRATION_TODO: Lower to a more acceptable value once other work is done
		double MaxSyncTimeSeconds = 50; //seconds
		GConfig->GetDouble(TEXT("HTTP"), TEXT("MaxSyncTimeSeconds"), MaxSyncTimeSeconds, GEngineIni);

		double SyncSleepTime = 0.25; //seconds
		GConfig->GetDouble(TEXT("HTTP"), TEXT("MaxSyncTimeSeconds"), MaxSyncTimeSeconds, GEngineIni);
		
		while(!bRequestFinished && CurrentWaitTime < MaxSyncTimeSeconds)
		{
			const double AppTime = FPlatformTime::Seconds();
			auto& manager = FHttpModule::Get().GetHttpManager();
			manager.FlushTick(AppTime - LastTime);
			LastTime = AppTime;
			CurrentWaitTime = AppTime - BeginWaitTime;
			FPlatformProcess::Sleep(SyncSleepTime);
		}

		if(CurrentWaitTime >= MaxSyncTimeSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to complete synchronous cloud transaction before return, endpoint=%s"), *Endpoint);
		}
	}
	return rtn;
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
			if (Payload.IsValid() &&
				FJsonObjectConverter::JsonObjectToUStruct<FModumateUserVerifyParams>(Payload.ToSharedRef(), &verifyParams) &&
				!verifyParams.AuthToken.IsEmpty())
			{
				SharedThis->AuthToken = verifyParams.AuthToken;
				SharedThis->AuthTokenTimestamp = FDateTime::Now();
				SharedThis->LoginStatus = ELoginStatus::Connected;
			}
			else
			{
				SharedThis->LoginStatus = ELoginStatus::ConnectionError;
			}

			if (Callback)
			{
				Callback(bSuccess, Payload);
			}
		},

		ServerErrorCallback,
		false
	);

	return false;
}

void FModumateCloudConnection::OnLogout()
{
	SetAuthToken(FString());
	SetRefreshToken(FString());
	LoginStatus = ELoginStatus::Disconnected;
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

			if (Callback)
			{
				Callback(bSuccess, Payload);
			}
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

			if (ServerErrorCallback)
			{
				ServerErrorCallback(ErrorCode, ErrorString);
			}
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

void FModumateCloudConnection::SetupRequestAuth(FHttpRequestRef& Request)
{
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));

	// The auth token is for authenticating and identifying users while they make secure requests to our AMS
	if (!AuthToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AuthToken);
	}

	// The x-api-key is for authenticating privileged services that make secure requests to our AMS in order to access non-user-specific data
	if (!XApiKey.IsEmpty())
	{
		Request->SetHeader(TEXT("x-api-key"), XApiKey);
	}
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
		TSharedPtr<FModumateCloudConnection> sharedThis = weakThisCaptured.Pin();

		if (bWasSuccessful && sharedThis.IsValid() && Response.IsValid())
		{
			int32 code = Response->GetResponseCode();
			FString content = Response->GetContentAsString();

			// If there's an automation handler, then store the response we get for this request so we can potentially play it back.
			if (sharedThis->AutomationHandler && Request.IsValid() && (curRequestAutomationIdx != INDEX_NONE))
			{
				sharedThis->AutomationHandler->RecordResponse(Request.ToSharedRef(), curRequestAutomationIdx, bWasSuccessful, code, content);
			}

			sharedThis->HandleRequestResponse(Callback, ServerErrorCallback, bRefreshTokenOnAuthFailure, bWasSuccessful, code, content);
		}
	});

	SetupRequestAuth(Request);
	Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

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
		if (Callback)
		{
			Callback(true, responseValue ? responseValue->AsObject() : nullptr);
		}
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

		if (ServerErrorCallback)
		{
			ServerErrorCallback(ResponseCode, responseValue && responseValue->AsObject() ? responseValue->AsObject()->GetStringField(TEXT("error")) : TEXT("null response"));
		}
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

bool FModumateCloudConnection::DownloadProject(const FString& ProjectID, const FProjectCallback& DownloadCallback, const FErrorCallback& ServerErrorCallback)
{
	if (PendingProjectDownloads.Contains(ProjectID) || ProjectID.IsEmpty())
	{
		return false;
	}

	auto request = FHttpModule::Get().CreateRequest();

	SetupRequestAuth(request);
	FString downloadEndpoint = FProjectConnectionHelpers::MakeProjectDataEndpoint(ProjectID);
	request->SetURL(GetCloudAPIURL() + downloadEndpoint);
	request->SetVerb(GetRequestTypeString(FModumateCloudConnection::Get));
	request->SetHeader(TEXT("Accepts"), TEXT("application/octet-stream"));
	request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	TWeakPtr<FModumateCloudConnection> weakThisCaptured(AsShared());
	request->OnProcessRequestComplete().BindLambda([weakThisCaptured, ProjectID, DownloadCallback, ServerErrorCallback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
		auto sharedThis = weakThisCaptured.Pin();
		if (ensure(sharedThis.IsValid()))
		{
			int32 code = Response->GetResponseCode();
			if (bWasSuccessful && (code == EHttpResponseCodes::Ok))
			{
				int32 responseSize = Response->GetContentLength();
				bool bEmptyProject = (responseSize == 0);
				UE_LOG(LogTemp, Log, TEXT("Successfully downloaded %.1fkB for Project ID: %s"), responseSize / 1024.0f, *ProjectID);
				FModumateDocumentHeader docHeader;
				FMOIDocumentRecord docRecord;
				if (bEmptyProject || FModumateSerializationStatics::LoadDocumentFromBuffer(Response->GetContent(), docHeader, docRecord))
				{
					if (DownloadCallback)
					{
						DownloadCallback(docHeader, docRecord, bEmptyProject);
					}
				}
				else if (ServerErrorCallback)
				{
					ServerErrorCallback(EHttpResponseCodes::ServerError, FString(TEXT("Project failed to deserialize!")));
				}
			}
			else
			{
				FString content = Response->GetContentAsString();
				UE_LOG(LogTemp, Error, TEXT("Error code %d when trying to download Project ID %s: %s"), code, *ProjectID, *content);
				if (ServerErrorCallback)
				{
					ServerErrorCallback(code, content);
				}
			}

			sharedThis->PendingProjectDownloads.Remove(ProjectID);
		}
		});

	if (request->ProcessRequest())
	{
		PendingProjectDownloads.Add(ProjectID);
		return true;
	}

	return false;
}

bool FModumateCloudConnection::UploadProject(const FString& ProjectID, const FModumateDocumentHeader& DocHeader, const FMOIDocumentRecord& DocRecord,
	const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	if (PendingProjectUploads.Contains(ProjectID))
	{
		return false;
	}

	PendingProjectUploads.Add(ProjectID);

	FString uploadEndpoint = FProjectConnectionHelpers::MakeProjectDataEndpoint(ProjectID);
	TWeakPtr<FModumateCloudConnection> weakThisCaptured(AsShared());
	bool bRequestSuccess = RequestEndpoint(uploadEndpoint, FModumateCloudConnection::Post,
		[&DocHeader, &DocRecord](FHttpRequestRef& RefRequest) {
			TArray<uint8> docBuffer;
			if (!FModumateSerializationStatics::SaveDocumentToBuffer(DocHeader, DocRecord, docBuffer))
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to save document to buffer in preparation for upload!"));
			}

			RefRequest->SetHeader(FString(TEXT("Content-Type")), FString(TEXT("application/octet-stream")));
			RefRequest->SetContent(docBuffer);
		},
		[weakThisCaptured, ProjectID, Callback](bool bSuccessful, const TSharedPtr<FJsonObject>& Response) {
			auto sharedThis = weakThisCaptured.Pin();
			if (!ensure(sharedThis.IsValid() && sharedThis->PendingProjectUploads.Contains(ProjectID)))
			{
				return;
			}

			sharedThis->PendingProjectUploads.Remove(ProjectID);
			if (Callback)
			{
				Callback(bSuccessful, Response);
			}
		},
		[weakThisCaptured, ProjectID, ServerErrorCallback](int32 ErrorCode, const FString& ErrorMessage) {
			auto sharedThis = weakThisCaptured.Pin();
			if (!ensure(sharedThis.IsValid() && sharedThis->PendingProjectUploads.Contains(ProjectID)))
			{
				return;
			}

			UE_LOG(LogTemp, Error, TEXT("Error code %d when trying to upload Project ID %s: %s"), ErrorCode, *ProjectID, *ErrorMessage);
			sharedThis->PendingProjectUploads.Remove(ProjectID);
			if (ServerErrorCallback)
			{
				ServerErrorCallback(ErrorCode, ErrorMessage);
			}
		}, false);

	if (!bRequestSuccess)
	{
		PendingProjectUploads.Remove(ProjectID);
	}

	return bRequestSuccess;
}

bool FModumateCloudConnection::ReadyForConnection(const FString& ProjectID)
{
#if UE_SERVER
	UE_LOG(LogTemp, Log, TEXT("Notifying cloud that project %s is ready for connections"), *ProjectID);
	
	FString endpoint = TEXT("/projects/") + ProjectID + TEXT("/status");
	if (!RequestEndpoint(endpoint, ERequestType::Post,
		[](FHttpRequestRef& RefRequest)
		{
			RefRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			RefRequest->SetContentAsString(TEXT("{\"status\": \"ready\"}"));
		},

		[](bool bSuccess, const TSharedPtr<FJsonObject>& Payload) { ensure(bSuccess); },

		[](int32 ErrorCode, const FString& ErrorMessage)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to report server ready error: %d - %s"), ErrorCode, *ErrorMessage);
		},
		false))
	{
		UE_LOG(LogTemp, Error, TEXT("ReportMultiPlayerFailure: Failed to notify cloud that server is ready for connections"));
		return false;
	}
#endif
	return true;
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

void FModumateCloudConnection::ReportMultiPlayerFailure(const FString& Category, const FString& Details, const FString& NetworkInfo /*= FString()*/)
{
#if UE_SERVER
	return;
#endif

	UE_LOG(LogTemp, Warning, TEXT("Reporting multiplayer error %s: \"%s\", %s"), *Category, *Details, *NetworkInfo);

	FMultiplayerError errorReport = { Category, Details, NetworkInfo };
	FString jsonReport;
	if (!ensure(WriteJsonGeneric(jsonReport, &errorReport)))
	{
		return;
	}

	if (!RequestEndpoint(TEXT("/networkreport"), ERequestType::Post,
		[&jsonReport](FHttpRequestRef& RefRequest)
		{
			RefRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			RefRequest->SetContentAsString(jsonReport);
		},

		[](bool bSuccess, const TSharedPtr<FJsonObject>& Payload) { ensure(bSuccess); },

		[](int32 ErrorCode, const FString& ErrorMessage)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to report network error: %d - %s"), ErrorCode, *ErrorMessage);
		},
		false))
	{
		UE_LOG(LogTemp, Error, TEXT("ReportMultiPlayerFailure: Failed to create error-report request"));
	}
}

bool FModumateCloudConnection::UploadPresetThumbnailImage(const FString& FileName, TArray<uint8>& OutData, const FSuccessCallback& Callback, const FErrorCallback& ServerErrorCallback)
{
	#if UE_SERVER
		return;
	#endif

	return RequestEndpoint(TEXT("/assets/presets/") + FileName + TEXT("/thumbnail"),
		ERequestType::Post,
		[this, OutData](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{
			// set up the auth
			SetupRequestAuth(RefRequest);

			// set png information
			RefRequest->SetHeader(TEXT("Content-Type"), TEXT("image/png"));
			RefRequest->SetContent(OutData);
		},
		Callback,
		ServerErrorCallback
	);
}