// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAccountManager.h"
#include "JsonUtilities.h"
#include "Dom/JsonObject.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Online/ModumateCloudConnection.h"

FModumateAccountManager::FModumateAccountManager(TSharedPtr<FModumateCloudConnection>& InConnection) :
	CloudConnection(InConnection)
{}

void FModumateAccountManager::RequestStatus()
{
	if (!CloudConnection->IsLoggedIn())
	{
		return;
	}

	TWeakPtr<FModumateAccountManager> WeakThisCaptured(AsShared());

	CloudConnection->RequestEndpoint(TEXT("/status"), FModumateCloudConnection::Get,
		[](TSharedRef<IHttpRequest>& RefRequest) 
		{},		

		[WeakThisCaptured](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (bSuccessful)
			{
				TSharedPtr<FModumateAccountManager> SharedThis = WeakThisCaptured.Pin();
				if (!SharedThis.IsValid())
				{
					return;
				}

				TSharedPtr<FJsonObject> userInfoJson = Response->GetObjectField(TEXT("user"));
				FModumateUserInfo userInfo;
				if (FJsonObjectConverter::JsonObjectToUStruct<FModumateUserInfo>(userInfoJson.ToSharedRef(), &userInfo))
				{
					SharedThis->SetUserInfo(userInfo);
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

	if (!UserStatus.Active)
	{
		CloudConnection->SetLoginStatus(ELoginStatus::UserDisabled);
	}
}

