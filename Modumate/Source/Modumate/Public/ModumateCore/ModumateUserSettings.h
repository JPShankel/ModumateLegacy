// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateUserSettings.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FRecentProject
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString ProjectPath;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FDateTime Timestamp;
};

USTRUCT(BlueprintType)
struct MODUMATE_API FModumateUserSettings
{
	GENERATED_BODY();

	static const FString FileName;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Version = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FRecentProject> RecentProjects;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool AutoBackup = true;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 AutoBackupFrequencySeconds = 60;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 TelemetryUploadFrequencySeconds = 60;

	bool bLoaded = false;
	bool bDirty = false;

	static FString GetLocalSavePath();
	static FString GetLocalTempDir();
	static FString GetTutorialsFolderPath();

	void RecordRecentProject(const FString &projectPath, bool bAutoSave = true);

	bool SaveLocally();
	bool LoadLocally();
};
