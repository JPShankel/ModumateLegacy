// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateUpdater.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Online/ModumateAccountManager.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Online/ModumateAnalyticsStatics.h"

#define LOCTEXT_NAMESPACE "ModumateUpdater"

const FString FModumateUpdater::InstallerSubdir(TEXT("Installers"));

TAutoConsoleVariable<int32> CVarModumateDisableForcedUpdate(
	TEXT("modumate.DisableForcedUpdate"),
	0,
	TEXT("Don't force the user to update if an installer is found."),
	ECVF_Default);

TAutoConsoleVariable<bool> CVarModumateQueryUpdates(
	TEXT("modumate.QueryUpdates"),
#if UE_BUILD_SHIPPING
	true,
#else
	false,
#endif
	TEXT("Whether to query for available installers for newer versions of the app."),
	ECVF_Default);

FModumateUpdater::~FModumateUpdater()
{
	if (PendingRequest.IsValid())
	{
		PendingRequest->CancelRequest();
	}
}

void FModumateUpdater::ProcessLatestInstallers(const FModumateUserStatus& Status)
{
	const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
	OurVersion = projectSettings->ProjectVersion;
	LatestVersion = Status.latest_modumate_version;

	if (CVarModumateQueryUpdates.GetValueOnAnyThread())
	{
		FetchInstallersObject();
	}
	else
	{
		SetUserLoggedIn();
	}
}

FString FModumateUpdater::GetPathForInstaller(const FString& CurrentVersion, const FString& NewestVersion) const
{
	FString path = FModumateUserSettings::GetLocalTempDir() / InstallerSubdir / NewestVersion / CurrentVersion;
	return path;
}

bool FModumateUpdater::StartDownload(const FString& Url, const FString& Location)
{
	if (!ensure(IFileManager::Get().MakeDirectory(*FPaths::GetPath(Location), true)) )
	{
		return false;
	}

	auto Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(Url);
	Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
	Request->SetHeader(TEXT("Accepts"), TEXT("application/octet-stream"));

	Request->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{ RequestCompleteCallback(Request, Response, bWasSuccessful); });

	if (Request->ProcessRequest())
	{
		PendingRequest = Request;
		return true;
	}

	return false;
}

void FModumateUpdater::RequestCompleteCallback(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (State == Downloading && bWasSuccessful && Request->GetStatus() == EHttpRequestStatus::Succeeded
		&& Response->GetContent().Num() != 0)
	{
		FFileHelper::SaveArrayToFile(Response->GetContent(), *DownloadFilename);
		State = Ready;
		NotifyUser();
	}
	else
	{
		State = Failed;
	}

	PendingRequest.Reset();
}

void FModumateUpdater::NotifyUser()
{
	if (State == Ready)
	{
		const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
		const FString& currentVersion = projectSettings->ProjectVersion;

		auto response = FPlatformMisc::MessageBoxExt(EAppMsgType::OkCancel,
			*FText::Format(LOCTEXT("PatchReleaseUpdatePrompt", "A new release of Modumate (v{0}) has been downloaded (you are currently using v{1}). "
				"Please upgrade to continue."), FText::FromString(DownloadVersion), FText::FromString(currentVersion)).ToString(),
			*LOCTEXT("UpdatePromptTitle", "Modumate Upgrade").ToString());

		if (response == EAppReturnType::Ok)
		{
			// Run via cmd as it's a UAC executable.
			FPlatformProcess::CreateProc(TEXT("cmd.exe"), *(TEXT("/c \"") + DownloadFilename + TEXT("\"")), false, false, false, nullptr, 0, TEXT("/"), nullptr);
			State = Upgrading;
		}
		else
		{
			State = Declined;
			UModumateAnalyticsStatics::RecordEventCustomFloat(GameInstance->GetWorld(), EModumateAnalyticsCategory::Session, TEXT("DeclinedPatchUpgrade"), 0.0f);
		}

		if (State != Declined || !CVarModumateDisableForcedUpdate.GetValueOnAnyThread())
		{
			auto playerController = Cast<AEditModelPlayerController>(GameInstance->GetPrimaryPlayerController());
			if (playerController)
			{
				playerController->CheckSaveModel();
			}
			FPlatformMisc::RequestExit(false);
		}
	}
}

void FModumateUpdater::FetchInstallersObject()
{
	auto cloud = GameInstance->GetCloudConnection();
	TWeakPtr<FModumateUpdater> weakThis(AsShared());

	cloud->RequestEndpoint(TEXT("/"), FModumateCloudConnection::Get,
		[cloud](TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& RefRequest)
		{
			RefRequest.Get().SetURL(cloud->GetCloudRootURL() + TEXT("/static/installers"));
			RefRequest.Get().SetHeader(TEXT("Authorization"), TEXT(""));
		},
		[weakThis](bool bWasSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			auto pinnedThis(weakThis.Pin());
			if (pinnedThis)
			{
				pinnedThis->InstallersObjectCallback(bWasSuccessful, Response);
			}
		},
		[weakThis](int32 ErrorCode, const FString& ErrorMessage)
		{
			auto pinnedThis(weakThis.Pin());
			if (pinnedThis)
			{
				pinnedThis->SetUserLoggedIn();
			}
			UE_LOG(LogTemp, Warning, TEXT("%d error querying for installers: \"%s\""), ErrorCode, *ErrorMessage);
		});
}

void FModumateUpdater::InstallersObjectCallback(bool bWasSuccessful, const TSharedPtr<FJsonObject>& Response)
{
	SetUserLoggedIn();
	FModumateInstallersObject installersObject;
	if (ensure(bWasSuccessful
		&& FJsonObjectConverter::JsonObjectToUStruct<FModumateInstallersObject>(Response.ToSharedRef(), &installersObject)) )
	{
		FString installerUrl;
		FString baseInstallerUrl;
		for (const auto& i: installersObject.Installers)
		{
			if (i.Version == OurVersion)
			{
				installerUrl = i.Url;
				break;
			}
			else if (i.Version.IsEmpty())
			{
				baseInstallerUrl = i.Url;
			}
		}

		if (installerUrl.IsEmpty())
		{
			if (!baseInstallerUrl.IsEmpty() && OurVersion != LatestVersion)
			{
				RequestUserDownload(baseInstallerUrl);
			}
			return;
		}

		if (installerUrl.IsEmpty())
		{
			// Client is out-of-date but no suitable installer for current version.
			return;
		}

		FString downloadLocation = GetPathForInstaller(OurVersion, LatestVersion) / TEXT("ModumateSetup.exe");
		if (IFileManager::Get().FileExists(*downloadLocation))
		{
			State = Ready;
			DownloadFilename = downloadLocation;
			DownloadVersion = LatestVersion;
			NotifyUser();
		}
		else
		{
			if (StartDownload(installerUrl, downloadLocation))
			{
				DownloadFilename = downloadLocation;
				DownloadVersion = LatestVersion;
				State = Downloading;
			}
		}

	}
}

void FModumateUpdater::RequestUserDownload(const FString& Url)
{
	auto response = FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
		*FText::Format(LOCTEXT("FullReleaseDownloadPrompt", "A new release of Modumate (v{0}) is available. Please click on OK to download."),
			FText::FromString(LatestVersion)).ToString(),
		*LOCTEXT("UpdatePromptTitle", "Modumate Upgrade").ToString());
			
	if (response == EAppReturnType::Ok || !CVarModumateDisableForcedUpdate.GetValueOnAnyThread())
	{
		FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
	}
	else
	{
		UModumateAnalyticsStatics::RecordEventCustomFloat(GameInstance->GetWorld(), EModumateAnalyticsCategory::Session, TEXT("DeclinedUpgradeDownload"), 0.0f);
	}

	auto playerController = Cast<AEditModelPlayerController>(GameInstance->GetPrimaryPlayerController());
	if (playerController)
	{
		playerController->CheckSaveModel();
	}
	FPlatformMisc::RequestExit(false);
}

void FModumateUpdater::SetUserLoggedIn()
{
	auto cloudConnection = GameInstance->GetCloudConnection();
	if (cloudConnection && cloudConnection->GetLoginStatus() != ELoginStatus::UserDisabled)
	{
		cloudConnection->SetLoginStatus(ELoginStatus::Connected);
	}
}

#undef LOCTEXT_NAMESPACE
