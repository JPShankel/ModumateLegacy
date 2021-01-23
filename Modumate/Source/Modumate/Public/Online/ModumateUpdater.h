// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Online/ModumateCloudConnection.h"

struct FModumateUserStatus;

class MODUMATE_API FModumateUpdater
{
public:
	FModumateUpdater(UModumateGameInstance* InGameInstance)
		: GameInstance(InGameInstance)
	{ }
	~FModumateUpdater();
	void ProcessLatestInstallers(const FModumateUserStatus& Status);

private:
	FString GetPathForInstaller(const FString& OurVersion, const FString& LatestVersion) const;
	bool StartDownload(const FString& Url, const FString& Location);
	void RequestCompleteCallback(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void NotifyUser();

	FString DownloadFilename;
	FString DownloadVersion;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingRequest;
	UModumateGameInstance* GameInstance;
	enum EState {Idle, Downloading, Ready, Failed, Declined, Upgrading} State = Idle;

	static const FString InstallerSubdir;
};
