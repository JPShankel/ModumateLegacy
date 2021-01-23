// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateUpdater.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Online/ModumateAccountManager.h"
#include "UnrealClasses/EditModelPlayerController.h"

const FString FModumateUpdater::InstallerSubdir(TEXT("Installers"));

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
	const FString& ourVersion = projectSettings->ProjectVersion;
	const FString& latestVersion = Status.latest_modumate_version;

	if (State == Idle && !latestVersion.IsEmpty() && latestVersion != ourVersion)
	{
		FString installerUrl;
		FString baseInstallerUrl;
		for (const auto& i: Status.Installers)
		{
			if (i.Version == ourVersion)
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
			installerUrl = baseInstallerUrl;
		}

		if (!ensure(!installerUrl.IsEmpty()) )
		{
			// Client is out-of-date but no suitable installer for current version.
			return;
		}

		FString downloadLocation = GetPathForInstaller(ourVersion, latestVersion) / TEXT("ModumateSetup.exe");
		if (IFileManager::Get().FileExists(*downloadLocation))
		{
			State = Ready;
			DownloadFilename = downloadLocation;
			DownloadVersion = latestVersion;
			NotifyUser();
		}
		else
		{
			if (StartDownload(installerUrl, downloadLocation))
			{
				DownloadFilename = downloadLocation;
				DownloadVersion = latestVersion;
				State = Downloading;
			}
		}
	}
}

FString FModumateUpdater::GetPathForInstaller(const FString& OurVersion, const FString& LatestVersion) const
{
	FString path = FModumateUserSettings::GetLocalTempDir() / InstallerSubdir / LatestVersion / OurVersion;
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

		auto response = Modumate::PlatformFunctions::ShowMessageBox(FString::Printf(
			TEXT("A new release of Modumate (v%s) has been downloaded (you are currently using v%s). "
				"We recommend upgrading immediately.\n\nDo you wish to upgrade now?"), *DownloadVersion, *currentVersion),
			TEXT("Modumate Upgrade"), Modumate::PlatformFunctions::OkayCancel);
		if (response == Modumate::PlatformFunctions::Yes)
		{
			// Run via cmd as it's a UAC executable.
			FPlatformProcess::CreateProc(TEXT("cmd.exe"), *(TEXT("/c \"") + DownloadFilename + TEXT("\"")), false, false, false, nullptr, 0, TEXT("/"), nullptr);
			State = Upgrading;
			auto playerController = Cast<AEditModelPlayerController>(GameInstance->GetPrimaryPlayerController());
			if (playerController)
			{
				playerController->CheckSaveModel();
			}
			FPlatformMisc::RequestExit(false);
		}
		else
		{
			State = Declined;
		}
	}
}
