// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Runtime/Online/HTTP/Public/Http.h"

#include "ModumateAccountManager.generated.h"

USTRUCT()
struct MODUMATE_API FModumateLoginParams
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString key;

	UPROPERTY()
	FString username;

	UPROPERTY()
	FString password;
};

USTRUCT()
struct MODUMATE_API FModumateLoginResponse
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString idToken;

	UPROPERTY()
	FString email;

	UPROPERTY()
	FString refreshToken;

	UPROPERTY()
	FString expiresIn;

	UPROPERTY()
	FString localId;

	UPROPERTY()
	FString registered;
};

USTRUCT()
struct MODUMATE_API FModumateUserInfo
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString firstname;

	UPROPERTY()
	FString lastname;

	UPROPERTY()
	FString email;

	UPROPERTY()
	FString uid;

	UPROPERTY()
	int points;

	UPROPERTY()
	FString zipcode;
};

USTRUCT()
struct MODUMATE_API FModumateTokenInfo
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString idToken;

	UPROPERTY()
	FString refreshToken;

	UPROPERTY()
	FModumateUserInfo user;
};

enum class ELoginStatus : uint8;

namespace Modumate
{
	class FModumateAccountManager
	{
	public:
		FModumateAccountManager();
		void Login(const FString& userName, const FString& password);

		ELoginStatus GetLoginStatus() const { return LoginStatus; }
		FString GetLocalId() const { return LocalId; }
		FString GetFirstname() const { return UserInfo.firstname; }
		FString GetLastname() const { return UserInfo.lastname; }
		FString GetEmail() const { return UserInfo.email; }

	private:
		void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
		void ProcessLogin(const FHttpResponsePtr Response);
		static FString ModumateIdentityEndpoint(const FString& api);

		ELoginStatus LoginStatus;
		FString IdToken;
		FString RefreshToken;
		FString LocalId;
		FModumateUserInfo UserInfo;

		static const FString ModumateIdentityURL;
		// API key from Josh.
		static const FString ApiKey;
	};
}