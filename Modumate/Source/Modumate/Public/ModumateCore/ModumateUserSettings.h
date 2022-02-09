// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/UnitConversion.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "DocumentManagement/ModumateWebProjectSettings.h"

#include "ModumateUserSettings.generated.h"

USTRUCT(BlueprintType)
struct MODUMATE_API FRecentProject
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString ProjectPath;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FDateTime Timestamp=FDateTime(0);
};

USTRUCT(BlueprintType)
struct MODUMATE_API FModumateGraphicsSettings
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bLit = true;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Shadows = 3;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 AntiAliasing = 3;

	// Unreal graphics settings: 0:low, 1:medium, 2:high, 3:epic, 4:cinematic
	static const int32 UnrealGraphicsSettingsMaxValue = 4;

	bool ToModumateWebProjectSettings(FModumateWebProjectSettings& Settings) const;
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

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 SaveFileUndoHistoryLength = 256;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FModumateGraphicsSettings GraphicsSettings;

	UPROPERTY()
	FString SavedUserName;

	UPROPERTY()
	FString SavedCredentials;

	bool bLoaded = false;
	bool bDirty = false;

	static FString GetLocalSavePath();
	static FString GetRestrictedSavePath();
	static FString GetLocalTempDir();

	void RecordRecentProject(const FString &projectPath, bool bAutoSave = true);

	bool SaveLocally();
	bool LoadLocally();
};
