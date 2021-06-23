// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Online/ModumateCloudConnection.h"

struct FModumateUserStatus;

class MODUMATE_API FModumateUpdater : public TSharedFromThis<FModumateUpdater>
{
public:
	FModumateUpdater(UModumateGameInstance* InGameInstance)
		: GameInstance(InGameInstance)
	{ }
	~FModumateUpdater();
	void ProcessLatestInstallers(const FModumateUserStatus& Status);

private:
	FString GetPathForInstaller(const FString& CurrentVersion, const FString& NewestVersion) const;
	bool StartDownload(const FString& Url, const FString& Location);
	void RequestCompleteCallback(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void NotifyUser();
	void FetchInstallersObject();
	void InstallersObjectCallback(bool bWasSuccessful, const TSharedPtr<FJsonObject>& Response);
	void RequestUserDownload(const FString& Url);
	void SetUserLoggedIn();

	FString OurVersion;
	FString LatestVersion;
	FString DownloadFilename;
	FString DownloadVersion;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingRequest;
	UModumateGameInstance* GameInstance;
	enum EState {Idle, Downloading, Ready, Failed, Declined, Upgrading} State = Idle;

	static const FString InstallerSubdir;
};
