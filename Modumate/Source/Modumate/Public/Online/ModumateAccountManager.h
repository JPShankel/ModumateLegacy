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
	FString Username;

	UPROPERTY()
	FString Password;
};

USTRUCT()
struct MODUMATE_API FModumateUserInfo
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString Firstname;

	UPROPERTY()
	FString Lastname;

	UPROPERTY()
	FString Email;

	UPROPERTY()
	FString Uid;

	UPROPERTY()
	int32 Points;

	UPROPERTY()
	FString Zipcode;
};

USTRUCT()
struct MODUMATE_API FModumateUserNotification
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString Text;

	UPROPERTY()
	FString Url;
};

USTRUCT()
struct MODUMATE_API FModumateUserStatus
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	bool Active;

	UPROPERTY()
	TArray<FModumateUserNotification> Notifications;

	UPROPERTY()
	TArray<FString> Permissions;

	UPROPERTY()
	FString latest_modumate_version;
};

USTRUCT()
struct MODUMATE_API FModumateUserVerifyParams
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FString IdToken;

	UPROPERTY()
	FString RefreshToken;

	UPROPERTY()
	FModumateUserInfo User;

	UPROPERTY()
	FModumateUserStatus Status;
};

enum class ELoginStatus : uint8;

UENUM(BlueprintType)
enum class EModumatePermission : uint8 { None, View, Edit, Save, Export };

class FModumateAccountManager
{
public:
	FModumateAccountManager();

	using FPermissionSet = TSet<EModumatePermission>;

	void Login(const FString& userName, const FString& password);

	ELoginStatus GetLoginStatus() const { return LoginStatus; }
	FString GetLocalId() const { return LocalId; }
	FString GetFirstname() const { return UserInfo.Firstname; }
	FString GetLastname() const { return UserInfo.Lastname; }
	FString GetEmail() const { return UserInfo.Email; }
	FString GetIdToken() const { return IdToken; }

	void RequestIdTokenRefresh(TBaseDelegate<void, bool>* callback = nullptr);
	void RequestStatus();
	bool IsloggedIn() const;
	bool HasPermission(EModumatePermission requestedPermission) const;

	void Tick();

	static FString GetAmsAddress();  // AMS address as URL.

	enum ResponseCodes {kSuccess = 200, kInvalidIdToken = 401, kBackendError = 463};

private:
	void OnAmsResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnStatusResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ProcessLogin(const FHttpResponsePtr Response);
	void ProcessUserStatus(const FModumateUserStatus& userStatus);
	static FString ModumateIdentityEndpoint(const FString& api);

	ELoginStatus LoginStatus;
	FString IdToken;
	FString RefreshToken;
	FString LocalId;
	FModumateUserInfo UserInfo;
	FString LatestVersion;
	TArray<TBaseDelegate<void, bool>> TokenRefreshDelegates;

	FDateTime IdTokenTimestamp;

	FPermissionSet CurrentPermissions;
	const static FTimespan IdTokenTimeout;
};
