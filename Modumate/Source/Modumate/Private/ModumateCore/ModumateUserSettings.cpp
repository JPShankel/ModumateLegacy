// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateUserSettings.h"

#include "JsonObjectConverter.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"


const FString FModumateUserSettings::FileName(TEXT("UserSettings.json"));

FString FModumateUserSettings::GetLocalSavePath()
{
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), FApp::GetProjectName(), FileName);
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

	// Only support Version 0 for now.
	bConversionSuccess = bConversionSuccess && ensureAlways(Version == 0);
	bLoaded = bConversionSuccess;

	return bConversionSuccess;
}
