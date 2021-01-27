// Copyright 2018 Modumate, Inc. All Rights Reserved.
#include "UnrealClasses/MainMenuGameMode.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "ModumateCore/PlatformFunctions.h"

#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateThumbnailHelpers.h"
#include "ModumateCore/ModumateUserSettings.h"


void AMainMenuGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	FString projectPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("-Project="), projectPath))
	{
		bool bIsTutorial = false;
		FParse::Bool(FCommandLine::Get(), TEXT("-IsTutorial="), bIsTutorial);

		UE_LOG(LogCallTrace, Log, TEXT("Command line specified project: \"%s\""), *projectPath);
		OpenProject(projectPath, bIsTutorial);
	}
	else
	{
		LoadRecentProjectData();
	}
}

void AMainMenuGameMode::LoadRecentProjectData()
{
	auto *gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance && gameInstance->UserSettings.bLoaded)
	{
		RecentProjectNames.Reset();
		ThumbnailBrushes.Reset();
		NumRecentProjects = 0;

		IFileManager &fileManger = IFileManager::Get();
		FModumateUserSettings &userSettings = gameInstance->UserSettings;
		static const int32 maxNumRecentProjects = 9;
		for (int32 i = 0; i < FMath::Min(maxNumRecentProjects, userSettings.RecentProjects.Num()); ++i)
		{
			FRecentProject &recentProject = userSettings.RecentProjects[i];

			if (fileManger.FileExists(*recentProject.ProjectPath))
			{
				FString projectJsonString;
				if (!FFileHelper::LoadFileToString(projectJsonString, *recentProject.ProjectPath))
				{
					continue;
				}

				TSharedPtr<FJsonObject> projectHeaderJson;
				auto jsonReader = TJsonReaderFactory<>::Create(projectJsonString);
				FString headerIdentifier(Modumate::DocHeaderField);
				if (!FJsonSerializer::Deserialize(jsonReader, projectHeaderJson, FJsonSerializer::EFlags::None, &headerIdentifier))
				{
					continue;
				}

				FModumateDocumentHeader projectHeader;
				if (!FJsonObjectConverter::JsonObjectToUStruct<FModumateDocumentHeader>(projectHeaderJson.ToSharedRef(), &projectHeader))
				{
					continue;
				}

				FString projectDir, projectName, projectExt;
				FPaths::Split(recentProject.ProjectPath, projectDir, projectName, projectExt);

				RecentProjectPaths.Add(recentProject.ProjectPath);
				RecentProjectNames.Add(FText::FromString(projectName));
				RecentProjectTimes.Add(recentProject.Timestamp);

				// Load the thumbnail resource from the base64-encoded image, and create a dynamic Slate brush for it if successful.
				FString thumbnailName = recentProject.ProjectPath;
				TSharedPtr<FSlateDynamicImageBrush> thumbnailBrush = FModumateThumbnailHelpers::LoadProjectThumbnail(projectHeader.Thumbnail, thumbnailName);
				ThumbnailBrushes.Add(thumbnailBrush);

				NumRecentProjects++;
			}
		}
	}
}

bool AMainMenuGameMode::GetRecentProjectData(int32 index, FString &outProjectPath, FText &outProjectName, FDateTime &outProjectTime, FSlateBrush &outDefaultThumbnail, FSlateBrush &outHoveredThumbnail) const
{
	if (index < NumRecentProjects)
	{
		outProjectPath = RecentProjectPaths[index];
		outProjectName = RecentProjectNames[index];
		outProjectTime = RecentProjectTimes[index];

		auto projectThumbnail = ThumbnailBrushes[index];
		outDefaultThumbnail = projectThumbnail.IsValid() ? *projectThumbnail.Get() : DefaultProjectThumbnail;
		outDefaultThumbnail.TintColor = DefaultThumbnailTint;

		outHoveredThumbnail = outDefaultThumbnail;
		outHoveredThumbnail.TintColor = DefaultThumbnailHoverTint;

		return true;
	}

	return false;
}

bool AMainMenuGameMode::OpenProject(const FString &projectPath, bool bIsTutorial)
{
	if (IFileManager::Get().FileExists(*projectPath))
	{
		const FName editModelLevelName(TEXT("EditModelLVL"));

		FString levelOptions = FString::Printf(TEXT("LoadFile=\"%s\"?IsTutorial=%s"), *projectPath,
			bIsTutorial ? *FCoreTexts::Get().True.ToString() : *FCoreTexts::Get().False.ToString());
		UGameplayStatics::OpenLevel(this, editModelLevelName, false, levelOptions);
		return true;
	}

	return false;
}

bool AMainMenuGameMode::OpenProjectFromPicker()
{
	FString projectPath;
	if (GetLoadFilename(projectPath))
	{
		return OpenProject(projectPath, false);
	}

	return false;
}

bool AMainMenuGameMode::GetLoadFilename(FString &loadFileName)
{
	return Modumate::PlatformFunctions::GetOpenFilename(loadFileName);
}

FDateTime AMainMenuGameMode::GetCurrentDateTime()
{
	return FDateTime::Now();
}


void AMainMenuGameMode::DisplayShutdownMessage(const FString &str, const FString &caption)
{
	Modumate::PlatformFunctions::ShowMessageBox(str, caption, Modumate::PlatformFunctions::EMessageBoxType::Okay);
}