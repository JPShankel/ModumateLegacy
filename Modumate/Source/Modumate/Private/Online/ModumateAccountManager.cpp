// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAccountManager.h"
#include "JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Online/ModumateUpdater.h"

FModumateAccountManager::FModumateAccountManager(TSharedPtr<FModumateCloudConnection>& InConnection,
	UModumateGameInstance* InGameInstance) :
	CloudConnection(InConnection),
	Updater(new FModumateUpdater(InGameInstance))
{}

FModumateAccountManager::~FModumateAccountManager() = default;

void FModumateAccountManager::RequestStatus()
{
	if (!CloudConnection->IsLoggedIn())
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
					SharedThis->ProcessUserStatus(status);
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
	return CloudConnection && CloudConnection->IsLoggedIn() && UserInfo.Analytics;
}

bool FModumateAccountManager::HasPermission(EModumatePermission requestedPermission) const
{
	return CurrentPermissions.Contains(requestedPermission);
}

void FModumateAccountManager::ProcessUserStatus(const FModumateUserStatus& UserStatus)
{
	// TODO: process announcements:
	CurrentPermissions.Empty();

	for (const FString& perm: UserStatus.Permissions)
	{
		auto permissionIndex = StaticEnum<EModumatePermission>()->GetValueByNameString(perm);
		if (permissionIndex != INDEX_NONE)
		{
			CurrentPermissions.Add(EModumatePermission(permissionIndex));
		}
	}

	FString version = UserStatus.latest_modumate_version;
	if (!version.IsEmpty())
	{
		LatestVersion = version;
	}

	Updater->ProcessLatestInstallers(UserStatus);

	if (!UserStatus.Active)
	{
		CloudConnection->SetLoginStatus(ELoginStatus::UserDisabled);
	}
}
