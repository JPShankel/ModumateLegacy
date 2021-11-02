// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateAccountManager.h"

#include "Dom/JsonObject.h"
#include "JsonUtilities.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Online/ModumateUpdater.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/MainMenuController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "UI/StartMenu/StartMenuWebBrowserWidget.h"
#include "Online/ModumateAnalyticsStatics.h"

#define LOCTEXT_NAMESPACE "ModumateAccountManager"

// From ModumateUpdater.cpp:
extern TAutoConsoleVariable<int32> CVarModumateDisableForcedUpdate;

FModumateAccountManager::FModumateAccountManager(TSharedPtr<FModumateCloudConnection>& InConnection,
	UModumateGameInstance* InGameInstance) :
	CloudConnection(InConnection),
	Updater(new FModumateUpdater(InGameInstance)),
	GameInstance(InGameInstance)
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

bool FModumateAccountManager::RequestServiceRemaining(const FString& ServiceName, const TFunction<void(FString, bool, bool, int32)>& Callback)
{
	if (!CloudConnection->IsLoggedIn() || !Callback)
	{
		return false;
	}

	FString endpointUrl;
	if (GetProjectID().IsEmpty())
	{
		endpointUrl = TEXT("/service/") + ServiceName + TEXT("/usage");
	}
	else
	{
		endpointUrl = TEXT("/projects/") + GetProjectID() + TEXT("/") + ServiceName + TEXT("/usage");
	}

	return CloudConnection->RequestEndpoint(endpointUrl, FModumateCloudConnection::Get,
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

void FModumateAccountManager::NotifyServiceUse(FString ServiceName, const TFunction<void(FString, bool)>& Callback)
{
	if (!CloudConnection->IsLoggedIn())
	{
		return;
	}

	FString endpointUrl;
	if (GetProjectID().IsEmpty())
	{
		endpointUrl = TEXT("/service/") + ServiceName + TEXT("/usage");
	}
	else
	{
		if (ServiceName == ServiceQuantityEstimates)
		{
			ServiceName = TEXT("qe");  // TODO: fix this inconsistency.
		}
		endpointUrl = TEXT("/projects/") + GetProjectID() + TEXT("/") + ServiceName + TEXT("/");
	}
	
	CloudConnection->RequestEndpoint(endpointUrl, FModumateCloudConnection::Post,
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

	CloudConnection->SetLoginStatus(CachedUserStatus.Active ? ELoginStatus::Connected : ELoginStatus::UserDisabled);

	FString version = CachedUserStatus.latest_modumate_version;
	if (!version.IsEmpty())
	{
		LatestVersion = version;
		const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
		OurVersion = projectSettings->ProjectVersion;
		if (OurVersion != LatestVersion && !CVarModumateDisableForcedUpdate.GetValueOnAnyThread())
		{
			PromptForUpgrade();
		}
	}
}

void FModumateAccountManager::PromptForUpgrade() const
{
	FString url = CloudConnection->GetInstallerDownloadURL();

	AMainMenuController* controller = CastChecked<AMainMenuController>(GameInstance->GetFirstLocalPlayerController());
	if (controller && controller->StartMenuWebBrowserWidget && controller->StartMenuWebBrowserWidget->ModalStatusDialog)
	{
		auto downloadAction = [url,controller, GameInstance = GameInstance]()
		{
			controller->ShutdownWebBrowser();
			GameInstance->bExitRequested = true;
			FPlatformProcess::LaunchURL(*url, nullptr, nullptr);
		};
		auto cancelAction = [GameInstance = GameInstance]()
		{
			UModumateAnalyticsStatics::RecordEventCustomFloat(GameInstance->GetWorld(), EModumateAnalyticsCategory::Session, TEXT("DeclinedUpgrade"), 0.0f);
		};
		
		FModalButtonParam cancelButton(EModalButtonStyle::Default, LOCTEXT("UpgradeDialogCancel", "Cancel"), cancelAction);
		FModalButtonParam downloadButton(EModalButtonStyle::Default, LOCTEXT("UpgradeDialogDownload", "Get Latest Version"), downloadAction);
		controller->StartMenuWebBrowserWidget->ModalStatusDialog->CreateModalDialog(LOCTEXT("UpgradeDialogTitle", "New Version"),
			FText::Format(LOCTEXT("UpgradeDialogMsg",
				"A new version (v{0}) of Modumate is available!\n"
				"You are currently using v{1}, so you should update Modumate to receive its latest features, fixes, and other quality-of-life improvements."),
				FText::FromString(LatestVersion), FText::FromString(OurVersion)),
			TArray<FModalButtonParam>({ downloadButton, cancelButton })
		);
	}
}

#undef LOCTEXT_NAMESPACE
