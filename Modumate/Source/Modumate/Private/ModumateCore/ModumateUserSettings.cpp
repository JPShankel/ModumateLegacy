// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateUserSettings.h"

#include "JsonObjectConverter.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "GenericPlatform/GenericPlatformDriver.h"

const FString FModumateUserSettings::FileName(TEXT("UserSettings.json"));



FString FModumateUserSettings::GetLocalSavePath()
{
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), FApp::GetProjectName(), FileName);
}

FString FModumateUserSettings::GetRestrictedSavePath()
{
	static const FString savedFolder(TEXT("Saved"));
	static const FString projectFolder(TEXT("LocalProjects"));
	static const FString restrictedSaveFile(TEXT("Free Project.mdmt"));
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), FApp::GetProjectName(), savedFolder, projectFolder, restrictedSaveFile);
}

FString FModumateUserSettings::GetLocalTempDir()
{
	return FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("Modumate"));
}

void FModumateUserSettings::RecordRecentProject(const FString &projectPath, bool bAutoSave)
{
	FDateTime currentTime = FDateTime::Now();
	bool bFoundInList = false;
	FString newProjectPath = FPaths::ConvertRelativePathToFull(projectPath);

	for (FRecentProject &recentProject : RecentProjects)
	{
		if (recentProject.ProjectPath == newProjectPath)
		{
			recentProject.Timestamp = currentTime;
			bFoundInList = true;
			break;
		}
	}

	if (!bFoundInList)
	{
		RecentProjects.Add(FRecentProject{ newProjectPath, currentTime });
	}

	RecentProjects.Sort([](const FRecentProject &p1, const FRecentProject &p2) {
		return p1.Timestamp > p2.Timestamp;
	});

	bDirty = true;

	if (bAutoSave)
	{
		SaveLocally();
	}
}

bool FModumateUserSettings::SaveLocally()
{
	FString settingsJsonString;
	bool bSerializeSuccess = FJsonObjectConverter::UStructToJsonObjectString<FModumateUserSettings>(*this, settingsJsonString);
	if (!bSerializeSuccess)
	{
		return false;
	}


	FString localSavePath = GetLocalSavePath();
	bool bSaveSuccess = FFileHelper::SaveStringToFile(settingsJsonString, *localSavePath);

	if (bSaveSuccess)
	{
		bDirty = false;
	}

	return bSaveSuccess;
}

bool FModumateUserSettings::LoadLocally()
{
	IFileManager &fileManager = IFileManager::Get();
	FString localSavePath = GetLocalSavePath();
	if (!fileManager.FileExists(*localSavePath))
	{
		SaveLocally();
		bLoaded = true;

		return false;
	}

	FString settingsJsonString;
	bool bLoadFileSuccess = FFileHelper::LoadFileToString(settingsJsonString, *localSavePath);
	if (!bLoadFileSuccess)
	{
		return false;
	}

	auto jsonReader = TJsonReaderFactory<>::Create(settingsJsonString);

	TSharedPtr<FJsonObject> settingsJson;
	bool bLoadJsonSuccess = FJsonSerializer::Deserialize(jsonReader, settingsJson) && settingsJson.IsValid();
	if (!bLoadJsonSuccess)
	{
		return false;
	}

	bool bConversionSuccess = FJsonObjectConverter::JsonObjectToUStruct<FModumateUserSettings>(settingsJson.ToSharedRef(), this);

	// To make sure that the local restricted project always shows up in the list of free projects, hack it into recent projects if it isn't already there.
	FString restrictedSavePath = GetRestrictedSavePath();
	if (fileManager.FileExists(*restrictedSavePath))
	{
		bool bRestrictedProjectInRecent = false;
		for (auto& recentProject : RecentProjects)
		{
			if (recentProject.ProjectPath == restrictedSavePath)
			{
				bRestrictedProjectInRecent = true;
				break;
			}
		}

		if (!bRestrictedProjectInRecent)
		{
			FRecentProject restrictedProjectEntry;
			restrictedProjectEntry.ProjectPath = restrictedSavePath;
			restrictedProjectEntry.Timestamp = FDateTime::Now();
			RecentProjects.Insert(restrictedProjectEntry, 0);
		}
	}

	// Only support Version 0 for now.
	bConversionSuccess = bConversionSuccess && ensureAlways(Version == 0);
	bLoaded = bConversionSuccess;

	return bConversionSuccess;
}

FModumateGraphicsSettings::FModumateGraphicsSettings()
{
	bRayTracingCapable = IsRayTracingCapable();
}

bool FModumateGraphicsSettings::IsRayTracingCapable()
{
	FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
	bRayTracingCapable = false;
	TArray<FString> compatibleCards;
	compatibleCards.Add(TEXT("RTX"));
	compatibleCards.Add(TEXT("GTX 1060"));
	compatibleCards.Add(TEXT("GTX 1070"));
	compatibleCards.Add(TEXT("GTX 1080"));
	if (GPUDriverInfo.IsNVIDIA())
	{
		for (int32 i = 0; i < compatibleCards.Num(); i++)
		{
			if (GPUDriverInfo.DeviceDescription.Contains(compatibleCards[i], ESearchCase::IgnoreCase))
			{
				bRayTracingCapable = true;
			}
		}
	}

	return bRayTracingCapable;
}

bool FModumateGraphicsSettings::ToWebProjectSettings(FWebProjectSettings& OutSettings) const
{
	OutSettings.shadows.value = FString::FromInt(Shadows);
	OutSettings.shadows.range = {0, UnrealGraphicsSettingsMaxValue};
	OutSettings.antiAliasing.value = FString::FromInt(AntiAliasing);
	OutSettings.antiAliasing.range = {0, UnrealGraphicsSettingsMaxValue};
	if (bRayTracingEnabled)
	{
		OutSettings.rayTracing.value = "true";
	}
	else
	{
		OutSettings.rayTracing.value = "false";
	}
	OutSettings.rayTracingQuality.value = FString::FromInt(RayTracingQuality);
	OutSettings.rayTracingQuality.range = { 0, UnrealGraphicsSettingsMaxValue };
	OutSettings.Exposure.value = FString::FromInt(ExposureValue);
	OutSettings.Exposure.range = { 0, 4 };
	if (bRayTracingCapable)
	{
		OutSettings.rayTracingCapable.value = "true";
	}
	else
	{
		OutSettings.rayTracingCapable.value = "false";
	}
	return true;
}

bool FModumateGraphicsSettings::FromWebProjectSettings(const FWebProjectSettings& InSettings)
{
	Shadows = FCString::Atoi(*InSettings.shadows.value);
	AntiAliasing = FCString::Atoi(*InSettings.antiAliasing.value);
	RayTracingQuality = FCString::Atoi(*InSettings.rayTracingQuality.value);
	ExposureValue = FCString::Atoi(*InSettings.Exposure.value);
	bRayTracingEnabled = InSettings.rayTracing.value.ToLower() == TEXT("true");
	return true;
}
