// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Runtime/Online/HTTP/Public/Http.h"

#include "ModumateAccountManager.generated.h"

USTRUCT()
struct MODUMATE_API FModumateLoginParams
{
	GENERATED_BODY();

	UPROPERTY()
	FString Username;

	UPROPERTY()
	FString Password;
};

USTRUCT()
struct MODUMATE_API FModumateUserInfo
{
	GENERATED_BODY();

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

	UPROPERTY()
	bool Analytics = false;
};

USTRUCT()
struct MODUMATE_API FModumateUserNotification
{
	GENERATED_BODY();

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString Text;

	UPROPERTY()
	FString Url;
};

USTRUCT()
struct MODUMATE_API FModumateInstallerItem
{
	GENERATED_BODY();

	UPROPERTY()
	FString Version;

	UPROPERTY()
	FString Url;
};

USTRUCT()
struct FModumateInstallersObject
{
	GENERATED_BODY();

	UPROPERTY()
	TArray<FModumateInstallerItem> Installers;
};

USTRUCT()
struct MODUMATE_API FModumateUserStatus
{
	GENERATED_BODY();

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
	GENERATED_BODY();

	UPROPERTY()
	FString AuthToken;

	UPROPERTY()
	FString RefreshToken;

	UPROPERTY()
	int32 LastDesktopLoginDateTime;

	UPROPERTY()
	FModumateUserInfo User;

	UPROPERTY()
	FModumateUserStatus Status;
};

USTRUCT()
struct MODUMATE_API FModumateAnalyticsEvent
{
	GENERATED_BODY();

	UPROPERTY()
	FString Key;

	UPROPERTY()
	float Value;

	UPROPERTY()
	int64 Timestamp;
};

enum class ELoginStatus : uint8;

UENUM(BlueprintType)
enum class EModumatePermission : uint8 { None, View, Edit, Save, Export };

class FModumateCloudConnection;
class FModumateUpdater;
class UModumateGameInstance;

class MODUMATE_API FModumateAccountManager : public TSharedFromThis<FModumateAccountManager>
{
public:
	FModumateAccountManager(TSharedPtr<FModumateCloudConnection>& InConnection, UModumateGameInstance * InGameInstance);
	~FModumateAccountManager();

	using FPermissionSet = TSet<EModumatePermission>;

	FString GetFirstname() const { return UserInfo.Firstname; }
	FString GetLastname() const { return UserInfo.Lastname; }
	FString GetEmail() const { return UserInfo.Email; }
	bool ShouldRecordTelemetry() const;

	void SetUserInfo(const FModumateUserInfo& InUserInfo) { UserInfo = InUserInfo; }
	void ProcessUserStatus(const FModumateUserStatus& UserStatus);

	void RequestStatus();
	bool HasPermission(EModumatePermission requestedPermission) const;

	bool IsFirstLogin() const { return bIsFirstLogin; }
	void SetIsFirstLogin(bool IsFirst) { bIsFirstLogin = IsFirst; }

	TSharedPtr<FModumateCloudConnection> CloudConnection;

private:
	TSharedPtr<FModumateUpdater> Updater;
	FModumateUserInfo UserInfo;
	FString LatestVersion;

	bool bIsFirstLogin = false;

	FPermissionSet CurrentPermissions;
};
