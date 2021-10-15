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

	UPROPERTY()
	FString RefreshToken;

	UPROPERTY()
	FString AppVersion;
};

USTRUCT()
struct MODUMATE_API FProjectPermissions
{
	GENERATED_BODY()

	UPROPERTY()
	bool CanView;

	UPROPERTY()
	bool CanEdit;

	UPROPERTY()
	bool CanChat;

	UPROPERTY()
	bool CanExport;
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
	FString ID;

	UPROPERTY()
	FString Zipcode;

	UPROPERTY()
	FString Photo;

	UPROPERTY()
	bool Analytics = false;

	UPROPERTY()
	FString Phone;
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
	bool Active = false;

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
	int32 LastDesktopLoginDateTime = 0;

	UPROPERTY()
	FModumateUserInfo User;
};

USTRUCT()
struct MODUMATE_API FModumateAnalyticsEvent
{
	GENERATED_BODY();

	UPROPERTY()
	FString Key;

	UPROPERTY()
	float Value = 0.0f;

	UPROPERTY()
	int64 Timestamp = 0;
};

USTRUCT()
struct MODUMATE_API FModumateServiceInfo
{
	GENERATED_BODY();

	UPROPERTY()
	bool Allowed = false;

	UPROPERTY()
	bool Limited = false;

	UPROPERTY()
	float Remaining = 0.0f;
};

enum class ELoginStatus : uint8;

// Must be kept up-to-date with the ModumateCloud backend, specifically the Config.PERMISSIONS strings.
UENUM(BlueprintType)
enum class EModumatePermission : uint8
{
	None,
	ProjectView,
	ProjectEdit,
	ProjectSave,
	ProjectSaveOne,
	ProjectNew,
	ProjectDelete,
	WorkspaceCreate,
	WorkspaceUpdate,
	WorkspaceInvite,
	WorkspaceRemove,
	ServiceJsontodwg
};

class FModumateCloudConnection;
class FModumateUpdater;
class UModumateGameInstance;

class MODUMATE_API FModumateAccountManager : public TSharedFromThis<FModumateAccountManager>
{
public:
	FModumateAccountManager(TSharedPtr<FModumateCloudConnection>& InConnection, UModumateGameInstance * InGameInstance);
	~FModumateAccountManager();

	FString GetFirstname() const { return CachedUserInfo.Firstname; }
	FString GetLastname() const { return CachedUserInfo.Lastname; }
	FString GetEmail() const { return CachedUserInfo.Email; }
	const FModumateUserInfo& GetUserInfo() const { return CachedUserInfo; }
	const FModumateUserStatus& GetUserStatus() const { return CachedUserStatus; }
	bool ShouldRecordTelemetry() const;

	void SetUserInfo(const FModumateUserInfo& InUserInfo);
	void ProcessUserStatus(const FModumateUserStatus& UserStatus, bool bQueryUpdateInstallers);

	void RequestStatus();

	bool IsFirstLogin() const { return bIsFirstLogin; }
	void SetIsFirstLogin(bool IsFirst) { bIsFirstLogin = IsFirst; }

	bool GetHasMultiplayerFeature() const { return bHasMultiplayerFeature; }
	void SetHasMultiplayerFeature(bool InHasMultiplayerFeature) { bHasMultiplayerFeature = InHasMultiplayerFeature; }

	FString GetProjectID() const { return ProjectID; }
	void SetProjectID(FString InProjectID) { ProjectID = MoveTemp(InProjectID); }

	// Request quota remaining for a particular service (eg. 'quantityestimates') from AMS.
	bool RequestServiceRemaining(const FString& ServiceName, const TFunction<void(FString, bool, bool, int32)>& Callback);

	// Notify server of a single use of a service against its quota.
	void NotifyServiceUse(FString ServiceName, const TFunction<void(FString, bool)>& Callback = nullptr);

	TSharedPtr<FModumateCloudConnection> CloudConnection;

	// Services:
	static const FString ServiceQuantityEstimates;
	static const FString ServiceDwg;

	FString CachedWorkspace;

private:
	void PromptForUpgrade() const;
	TSharedPtr<FModumateUpdater> Updater;
	UModumateGameInstance* GameInstance = nullptr;
	FModumateUserInfo CachedUserInfo;
	FModumateUserStatus CachedUserStatus;
	FString LatestVersion;
	FString OurVersion;
	FString ProjectID;  // If multiplayer

	bool bIsFirstLogin = false;
	bool bHasMultiplayerFeature = false; // Eventually this will not be needed as customers roll into full multiplayer
};
