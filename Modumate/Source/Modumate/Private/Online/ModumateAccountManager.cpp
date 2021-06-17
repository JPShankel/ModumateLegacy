// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAccountManager.h"

#include "Dom/JsonObject.h"
#include "JsonUtilities.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Online/ModumateUpdater.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"

FModumateAccountManager::FModumateAccountManager(TSharedPtr<FModumateCloudConnection>& InConnection,
	UModumateGameInstance* InGameInstance) :
	CloudConnection(InConnection),
	Updater(new FModumateUpdater(InGameInstance))
{}

FModumateAccountManager::~FModumateAccountManager() = default;

const FString FModumateAccountManager::ServiceQuantityEstimates(TEXT("quantityestimates"));
const FString FModumateAccountManager::ServiceDwg(TEXT("jsontodwg"));

void FModumateAccountManager::RequestStatus()
{
	if (!CloudConnection->IsLoggedIn(true))
	{
		return;
	}

	TWeakPtr<FModumateAccountManager> WeakThisCaptured(AsShared());

	CloudConnection->RequestEndpoint(TEXT("/status"), FModumateCloudConnection::Get,
		[](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{ },
		[WeakThisCaptured](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (bSuccessful && Response.IsValid())
			{
				TSharedPtr<FModumateAccountManager> SharedThis = WeakThisCaptured.Pin();
				if (!SharedThis.IsValid())
				{
					return;
				}

				FModumateUserStatus status;
				if (FJsonObjectConverter::JsonObjectToUStruct<FModumateUserStatus>(Response.ToSharedRef(),&status))
				{
					SharedThis->ProcessUserStatus(status, true);
				}
			}
		},
		
		[](int32 ErrorCode, const FString& ErrorString) 
		{
			UE_LOG(LogTemp, Error, TEXT("Request Status Error: %s"), *ErrorString);		
		}
	);
}
bool FModumateAccountManager::ShouldRecordTelemetry() const
{ 
	return CloudConnection && CloudConnection->IsLoggedIn() && CachedUserInfo.Analytics;
}

bool FModumateAccountManager::HasPermission(EModumatePermission requestedPermission) const
{
	return CurrentPermissions.Contains(requestedPermission);
}

bool FModumateAccountManager::RequestServiceRemaining(const FString& ServiceName, const TFunction<void(FString, bool, bool, int32)>& Callback)
{
	if (!CloudConnection->IsLoggedIn() || !Callback)
	{
		return false;
	}

	return CloudConnection->RequestEndpoint(TEXT("/service/") + ServiceName + TEXT("/usage"), FModumateCloudConnection::Get,
		[](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
			{ },
		[Callback, ServiceName](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (bSuccessful && Response.IsValid())
			{
				FModumateServiceInfo serviceInfo;
				if (FJsonObjectConverter::JsonObjectToUStruct<FModumateServiceInfo>(Response.ToSharedRef(), &serviceInfo))
				{
					Callback(ServiceName, true, serviceInfo.Limited, FMath::RoundToInt(serviceInfo.Remaining));
					return;
				}
			}
			Callback(ServiceName, false, false, 0);
		},
		[Callback, ServiceName](int32 ErrorCode, const FString& ErrorString)
		{
			Callback(ServiceName, false, false, 0);
		}
	);
}

void FModumateAccountManager::NotifyServiceUse(const FString& ServiceName, const TFunction<void(FString, bool)>& Callback)
{
	if (!CloudConnection->IsLoggedIn())
	{
		return;
	}
	
	CloudConnection->RequestEndpoint(TEXT("/service/") + ServiceName, FModumateCloudConnection::Post,
		[](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{},
		[Callback, ServiceName](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (!bSuccessful)
			{
				UE_LOG(LogTemp, Error, TEXT("FModumateAccountManager::NotifyServiceUse() unsuccessful"));
			}

			if (Callback)
			{
				Callback(ServiceName, bSuccessful);
			}
		},
		[Callback, ServiceName](int32 ErrorCode, const FString& ErrorString)
		{
			UE_LOG(LogTemp, Error, TEXT("Request Status Error: %s"), *ErrorString);
			if (Callback)
			{
				Callback(ServiceName, false);
			}
		}
	);
}

void FModumateAccountManager::SetUserInfo(const FModumateUserInfo& InUserInfo)
{
	CachedUserInfo = InUserInfo;

	static const FString crashDataKeyUserID(TEXT("ModUserID"));
	static const FString crashDataKeyUserEmail(TEXT("ModUserEmail"));

	FGenericCrashContext::SetGameData(crashDataKeyUserID, CachedUserInfo.ID);
	FGenericCrashContext::SetGameData(crashDataKeyUserEmail, CachedUserInfo.Email);
}

void FModumateAccountManager::ProcessUserStatus(const FModumateUserStatus& UserStatus, bool bQueryUpdateInstallers)
{
	CachedUserStatus = UserStatus;

	static const FString permissionSeparator(TEXT("."));
	static const FString permissionWildcard(TEXT("*"));

	// TODO: process announcements:
	CurrentPermissions.Empty();

	auto permissionEnum = StaticEnum<EModumatePermission>();
	int32 numPermissions = permissionEnum->NumEnums();
	if (permissionEnum->ContainsExistingMax())
	{
		numPermissions--;
	}

	// Add specific enum permissions to the local account based on incoming server-formatted permission strings,
	// which are lowercase and can have wildcards after the category.
	FString permissionCategory, permissionSpecific;
	for (const FString& permissionServerString: CachedUserStatus.Permissions)
	{
		if (permissionServerString.Split(permissionSeparator, &permissionCategory, &permissionSpecific) &&
			(permissionCategory.Len() > 0) && (permissionSpecific.Len() > 0))
		{
			permissionCategory[0] = FChar::ToUpper(permissionCategory[0]);
			permissionSpecific[0] = FChar::ToUpper(permissionSpecific[0]);

			if (permissionSpecific == permissionWildcard)
			{
				for (int32 permissionIdx = 0; permissionIdx < numPermissions; ++permissionIdx)
				{
					if (permissionEnum->GetNameStringByIndex(permissionIdx).StartsWith(permissionCategory))
					{
						CurrentPermissions.Add(static_cast<EModumatePermission>(permissionEnum->GetValueByIndex(permissionIdx)));
					}
				}
			}
			else
			{
				FString permissionEnumString = permissionCategory + permissionSpecific;
				int64 searchResult = permissionEnum->GetValueByNameString(permissionEnumString, EGetByNameFlags::CaseSensitive);
				if (searchResult != INDEX_NONE)
				{
					CurrentPermissions.Add(static_cast<EModumatePermission>(searchResult));
				}
			}
		}
	}

	FString version = CachedUserStatus.latest_modumate_version;
	if (!version.IsEmpty())
	{
		LatestVersion = version;
	}

#if PLATFORM_WINDOWS
	if (bQueryUpdateInstallers)
	{
		Updater->ProcessLatestInstallers(CachedUserStatus);
	}
#endif

	if (CachedUserStatus.Active)
	{
		CloudConnection->SetLoginStatus(ELoginStatus::Connected);
	}
	else
	{
		CloudConnection->SetLoginStatus(ELoginStatus::UserDisabled);
	}
}
