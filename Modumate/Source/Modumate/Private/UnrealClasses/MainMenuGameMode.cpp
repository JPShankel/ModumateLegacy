// Copyright 2018 Modumate, Inc. All Rights Reserved.
#include "UnrealClasses/MainMenuGameMode.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "ModumateCore/ModumateThumbnailHelpers.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateCloudConnection.h"
#include "UI/StartMenu/StartRootMenuWidget.h"
#include "UI/TutorialManager.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/MainMenuController.h"

#define LOCTEXT_NAMESPACE "ModumateMainMenu"

void AMainMenuGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	LoadRecentProjectData();

	// If there's a file to open, and there are saved credentials, then try to log in so the file can automatically be opened.
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance &&
		(!gameInstance->PendingProjectPath.IsEmpty() || !gameInstance->PendingInputLogPath.IsEmpty()) &&
		gameInstance->UserSettings.bLoaded &&
		!gameInstance->UserSettings.SavedUserName.IsEmpty() &&
		!gameInstance->UserSettings.SavedCredentials.IsEmpty())
	{
		gameInstance->Login(gameInstance->UserSettings.SavedUserName, FString(), gameInstance->UserSettings.SavedCredentials, true, true);
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
				FString headerIdentifier(FModumateSerializationStatics::DocHeaderField);
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

void AMainMenuGameMode::OnLoggedIn()
{
	UWorld* world = GetWorld();
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	ULocalPlayer* localPlayer = world ? world->GetFirstLocalPlayerFromController() : nullptr;
	AMainMenuController* controller = localPlayer ? Cast<AMainMenuController>(localPlayer->GetPlayerController(world)) : nullptr;
	if (!ensure(gameInstance && controller && controller->StartRootMenuWidget))
	{
		return;
	}

	// Show the start menu, even if we're just about to clear it by loading a project.
	controller->StartRootMenuWidget->ShowStartMenu();

	// If there's a pending project or input log that came from the command line, then open it directly.
	if (!gameInstance->PendingProjectPath.IsEmpty())
	{
		OpenProject(gameInstance->PendingProjectPath);
	}
	else if (!gameInstance->PendingInputLogPath.IsEmpty())
	{
		OpenEditModelLevel();
	}
	// Otherwise, if the user is trying to open a cloud project by ID, then attempt to open that connection.
	else if (!gameInstance->PendingProjectID.IsEmpty() && OpenCloudProject(gameInstance->PendingProjectID))
	{
		gameInstance->PendingProjectID.Empty();
		controller->StartRootMenuWidget->ShowModalStatus(LOCTEXT("InitialCloudProjectStatus", "Connecting to online project..."), false);
	}
	// Otherwise, potentially load the starting walkthrough
	else if (gameInstance->TutorialManager->CheckAbsoluteBeginner())
	{
		UE_LOG(LogTemp, Log, TEXT("Loading first-time beginner walkthrough..."));
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

void AMainMenuGameMode::OpenEditModelLevel()
{
	const FName editModelLevelName(TEXT("EditModelLVL"));
	UGameplayStatics::OpenLevel(this, editModelLevelName, false);
}

bool AMainMenuGameMode::OpenProject(const FString& ProjectPath)
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance && IFileManager::Get().FileExists(*ProjectPath))
	{
		gameInstance->PendingProjectPath = ProjectPath;

		AMainMenuController* controller = Cast<AMainMenuController>(gameInstance->GetFirstLocalPlayerController(GetWorld()));
		if (controller && controller->StartRootMenuWidget)
		{
			FText loadingStatusText = FText::Format(LOCTEXT("OpenLocalProjectFormat", "Opening project \"{0}\"..."),
				FText::FromString(FPaths::GetBaseFilename(ProjectPath)));
			controller->StartRootMenuWidget->ShowStartMenu();
			controller->StartRootMenuWidget->ShowModalStatus(loadingStatusText, false);
		}

		// Delay OpenEditModelLevel for a frame, to give Slate a chance to display the loading status widget.
		// OpenEditModelLevel currently load the document and does all of the heavy object cleaning in a single blocking callstack,
		// so this loading message will effectively display until the project is ready to use.
		// TODO: create the loading status widget from a new, separate thread so it can be animated, since Slate is already running outside of the main thread.
		gameInstance->GetTimerManager().SetTimerForNextTick([this]() { OpenEditModelLevel(); });

		return true;
	}

	return false;
}

bool AMainMenuGameMode::OpenCloudProject(const FString& ProjectID)
{
	if (!PendingCloudProjectID.IsEmpty())
	{
		return false;
	}

	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (!accountManager.IsValid() || !cloudConnection.IsValid() || !cloudConnection->IsLoggedIn())
	{
		return false;
	}

	auto weakThis = MakeWeakObjectPtr(this);
	bool bEndpointSuccess = cloudConnection->RequestEndpoint(
		FProjectConnectionHelpers::MakeProjectConnectionEndpoint(ProjectID),
		FModumateCloudConnection::ERequestType::Put,
		nullptr,
		[weakThis, ProjectID](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			FProjectConnectionResponse createConnectionResponse;
			if (!ensure(bSuccessful && Response.IsValid() && weakThis.IsValid() &&
				FJsonObjectConverter::JsonObjectToUStruct(Response.ToSharedRef(), &createConnectionResponse)))
			{
				UE_LOG(LogTemp, Error, TEXT("Unexpected error in response to creating a connection to project %s."), *ProjectID);
				return;
			}

			weakThis->OnCreatedProjectConnection(ProjectID, createConnectionResponse);
		},
		[weakThis, ProjectID](int32 ErrorCode, const FString& ErrorString)
		{
			if (weakThis.IsValid())
			{
				weakThis->OnProjectConnectionFailed(ProjectID, ErrorCode, ErrorString);
			}
		});

	if (bEndpointSuccess)
	{
		PendingCloudProjectID = ProjectID;
	}

	return bEndpointSuccess;
}

void AMainMenuGameMode::OnCreatedProjectConnection(const FString& ProjectID, const FProjectConnectionResponse& Response)
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (!accountManager.IsValid() || !cloudConnection.IsValid() || !cloudConnection->IsLoggedIn() || !ensure(ProjectID == PendingCloudProjectID))
	{
		return;
	}

	const FString& userID = accountManager->GetUserInfo().ID;
	cloudConnection->CacheEncryptionKey(userID, ProjectID, Response.Key);

	FString targetURL = FString::Printf(TEXT("%s:%d"), *Response.IP, Response.Port);
	OpenProjectServerInstance(targetURL, ProjectID);
}

void AMainMenuGameMode::OnProjectConnectionFailed(const FString& ProjectID, int32 ErrorCode, const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("Error code %d while trying to create a connection to project %s: %s"), ErrorCode, *ProjectID, *ErrorMessage);

	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto* playerController = gameInstance ? Cast<AMainMenuController>(gameInstance->GetFirstLocalPlayerController()) : nullptr;
	if (playerController && playerController->StartRootMenuWidget && ensure(ProjectID == PendingCloudProjectID))
	{
		PendingCloudProjectID.Empty();

		playerController->StartRootMenuWidget->ShowModalStatus(LOCTEXT("OpenProjectError", "Failed to open project! Please try again later."), true);
	}
}

bool AMainMenuGameMode::OpenProjectFromPicker()
{
	FString projectPath;
	if (GetLoadFilename(projectPath))
	{
		return OpenProject(projectPath);
	}

	return false;
}

bool AMainMenuGameMode::GetLoadFilename(FString &loadFileName)
{
	return FModumatePlatform::GetOpenFilename(loadFileName);
}

FDateTime AMainMenuGameMode::GetCurrentDateTime()
{
	return FDateTime::Now();
}

bool AMainMenuGameMode::OpenProjectServerInstance(const FString& URL, const FString& ProjectID)
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	auto* playerController = gameInstance ? Cast<AMainMenuController>(gameInstance->GetFirstLocalPlayerController()) : nullptr;
	if (!(accountManager && cloudConnection && cloudConnection->IsLoggedIn() && playerController))
	{
		return false;
	}

	// The public encryption token is just the user ID and the session ID, as used by Epic's examples in Fortnite
	FString fullURL = FString::Printf(TEXT("%s?EncryptionToken=%s"),
		*URL, *FModumateCloudConnection::MakeEncryptionToken(accountManager->GetUserInfo().ID, ProjectID));

	playerController->ClientTravel(fullURL, ETravelType::TRAVEL_Absolute);

	// TODO: update status and clear PendingCloudProjectID if traveling fails

	if (playerController->StartRootMenuWidget)
	{
		// TODO: format the text with the name of the project itself
		playerController->StartRootMenuWidget->ShowModalStatus(LOCTEXT("OpenProjectBegin", "Opening online project..."), false);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
