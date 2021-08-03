// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/ModumateGameInstance.h"

#include "Algo/Transform.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Dom/JsonObject.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/ModumateDraftingView.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateStats.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "UI/DimensionManager.h"
#include "UI/EditModelUserWidget.h"
#include "UI/TutorialManager.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/MainMenuGameMode.h"
#include "UnrealClasses/ThumbnailCacheManager.h"
#include "UnrealClasses/TooltipManager.h"
#include "Online/ModumateCloudConnection.h"
#include "Quantities/QuantitiesManager.h"

using namespace ModumateCommands;
using namespace ModumateParameters;


#define LOCTEXT_NAMESPACE "ModumateGameInstance"

const FString UModumateGameInstance::TestScriptRelativePath(TEXT("TestScripts"));

UModumateGameInstance::UModumateGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

UModumateDocument *UModumateGameInstance::GetDocument()
{
	AEditModelGameState *gameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AEditModelGameState>() : nullptr;
	if (gameState != nullptr)
	{
		return gameState->Document;
	}
	ensureAlways(false);
	return nullptr;
}

void UModumateGameInstance::Init()
{
	Super::Init();

	CloudConnection = MakeShared<FModumateCloudConnection>();
	AccountManager = MakeShared<FModumateAccountManager>(CloudConnection, this);
	AnalyticsInstance = UModumateAnalyticsStatics::InitAnalytics();

	UModumateFunctionLibrary::SetWindowTitle();

	RegisterAllCommands();

	// Load user settings during GameInstance initialization, before any GameModes might need its contents.
	UserSettings.LoadLocally();

	// Create and initialize our thumbnail cache manager, before anyone might make any requests for them
	ThumbnailCacheManager = NewObject<UThumbnailCacheManager>(this);
	ThumbnailCacheManager->Init();

	DraftingManager = NewObject<UDraftingManager>(this);
	DraftingManager->Init();

	DimensionManager = NewObject<UDimensionManager>(this);
	DimensionManager->Init();

	if (ensure(TooltipManagerClass))
	{
		TooltipManager = NewObject<UTooltipManager>(this, TooltipManagerClass);
		TooltipManager->Init();
	}

	if (ensure(TutorialManagerClass))
	{
		TutorialManager = NewObject<UModumateTutorialManager>(this, TutorialManagerClass);
		TutorialManager->Init();
	}

	QuantitiesManager = MakeShared<FQuantitiesManager>(this);

	// Initialize the Object Database, which can take a while
	ObjectDatabase = new FModumateDatabase();
	ObjectDatabase->Init();

	double databaseLoadTime = 0.0;
	{
		SCOPE_SECONDS_COUNTER(databaseLoadTime);
		ObjectDatabase->ReadPresetData();
	}
	UE_LOG(LogPerformance, Log, TEXT("Object database loaded in %d ms"), FMath::RoundToInt(1000.0 * databaseLoadTime));

	GetTimerManager().SetTimer(SlowTickHandle, this, &UModumateGameInstance::SlowTick, 0.5f, true);

	UEngine* engine = GetEngine();
	if (engine)
	{
		engine->TravelFailureEvent.AddUObject(this, &UModumateGameInstance::OnTravelFailure);
		engine->NetworkFailureEvent.AddUObject(this, &UModumateGameInstance::OnNetworkFailure);
	}
}

TSharedPtr<FModumateCloudConnection> UModumateGameInstance::GetCloudConnection() const
{
	return CloudConnection;
}

TSharedPtr<FQuantitiesManager> UModumateGameInstance::GetQuantitiesManager() const
{
	return QuantitiesManager;
}

TSharedPtr<FModumateAccountManager> UModumateGameInstance::GetAccountManager() const
{
	return AccountManager;
}

const AEditModelGameMode* UModumateGameInstance::GetEditModelGameMode() const
{
	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return nullptr;
	}

	auto gameModeClass = world->GetWorldSettings()->DefaultGameMode;
	return GetDefault<AEditModelGameMode>(gameModeClass);
}

void UModumateGameInstance::RegisterAllCommands()
{
	RegisterCommand(kBIMDebug, [this](const FModumateFunctionParameterSet& params, FModumateFunctionParameterSet& output)
	{
		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		if (controller && controller->EditModelUserWidget)
		{
			bool newShow = !controller->EditModelUserWidget->IsBIMDebuggerOn();
			controller->EditModelUserWidget->ShowBIMDebugger(newShow);
			return true;
		}
		return false;
	});

	RegisterCommand(kYield, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		float interval = params.GetValue(kSeconds);
		if (ConsoleWaitTimer.IsValid())
		{
			ConsoleWaitTimer.Invalidate();
		}

		TimerManager->SetTimer(
			ConsoleWaitTimer,
			[this]()
		{
			ConsoleWaitTimer.Invalidate();
			ProcessCommandQueue();
		},
			0.1f, false, interval);
		return true;
	});

	RegisterCommand(kCloneObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		FTransform tr = FTransform::Identity;
		tr.SetTranslation(params.GetValue(kDelta));
		output.SetValue(kObjectIDs, GetDocument()->CloneObjects(GetWorld(), params.GetValue(kObjectIDs),tr));
		return true;
	});

	RegisterCommand(kRestoreDeletedObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		GetDocument()->RestoreDeletedObjects(params.GetValue(kObjectIDs));
		return true;
	});

	RegisterCommand(kScreenPrint, [](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Black, params.GetValue(ModumateParameters::kText));
		return true;
	});

	RegisterCommand(kMakeScopeBox, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		UModumateDocument* doc = GetDocument();
		if (doc == nullptr)
		{
			return false;
		}

		TArray<FVector> points = params.GetValue(kControlPoints);
		FName assemblyKey = params.GetValue(kAssembly);
		float height = params.GetValue(kHeight);

		TArray<int32> newObjIDs;

		bool bSuccess = doc->MakeScopeBoxObject(GetWorld(), points, newObjIDs, height);
		output.SetValue(kObjectIDs, newObjIDs);

		return bSuccess;
	});

	RegisterCommand(kViewGroupObject, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		int32 objID = params.GetValue(kObjectID);
		FString idString = params.GetValue(kObjectID);
		AModumateObjectInstance *ob = nullptr;

		if (objID > 0)
		{
			ob = GetDocument()->GetObjectById(objID);
		}

		AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->SetViewGroupObject(ob);
		return true;
	});

	RegisterCommand(kDeleteObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		AEditModelPlayerController *playerController = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(playerController->PlayerState);
		GetDocument()->DeleteObjects(params.GetValue(kObjectIDs), true, params.GetValue(kIncludeConnected));
		return true;
	});

	RegisterCommand(kDeleteSelectedObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		AEditModelPlayerController *playerController = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(playerController->PlayerState);
		TArray<AModumateObjectInstance*> obs = playerState->SelectedObjects.Array();
		playerState->DeselectAll();

		if (playerController->ToolIsInUse())
		{
			playerController->AbortUseTool();
		}

		bool bAllowRoomAnalysis = true;
		bool bIncludeConnected = params.GetValue(kIncludeConnected);

		GetDocument()->DeleteObjects(obs, bAllowRoomAnalysis, bIncludeConnected);
		return true;
	});

	RegisterCommand(kDebug, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		AEditModelPlayerController* playerController = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState* playerState = playerController ? playerController->EMPlayerState : nullptr;
		if (playerState == nullptr)
		{
			return false;
		}

		FString type = params.GetValue(TEXT("type"),TEXT("document"));

		bool hasShow = params.HasValue(TEXT("on"));

		FModumateCommandParameter v = params.GetValue(TEXT("on"),true);
		bool show = v.AsBool();

		if (type.Equals(TEXT("mouse")))
		{
			playerState->ShowDebugSnaps = hasShow ? show : !playerState->ShowDebugSnaps;
		}
		else if (type.Equals(TEXT("document")))
		{
			playerState->ShowDocumentDebug = hasShow ? show : !playerState->ShowDocumentDebug;
		}
		else if (type.Equals(TEXT("graph")))
		{
			playerState->SetShowGraphDebug(hasShow ? show : !playerState->bShowGraphDebug);
		}
		else if (type.Equals(TEXT("volume")))
		{
			playerState->bShowVolumeDebug = hasShow ? show : !playerState->bShowVolumeDebug;
		}
		else if (type.Equals(TEXT("surface")))
		{
			playerState->bShowSurfaceDebug = hasShow ? show : !playerState->bShowSurfaceDebug;
		}
		else if (type.Equals(TEXT("ddl2")))
		{
			playerState->bDevelopDDL2Data = hasShow ? show : !playerState->bDevelopDDL2Data;
		}
		else if (type.Equals(TEXT("multiplayer")))
		{
			playerState->bShowMultiplayerDebug = hasShow ? show : !playerState->bShowMultiplayerDebug;
		}

		return true;
	});

	auto copySelected = [this]()
	{
		AEditModelPlayerState* playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->CopySelectedToClipboard(*GetDocument());
	};

	RegisterCommand(kCopySelected, [copySelected, this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		copySelected();
		return true;
	});

	RegisterCommand(kCutSelected, [copySelected, this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		copySelected();
		AEditModelPlayerState* playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		TArray<AModumateObjectInstance*> toBeCut = playerState->SelectedObjects.Array();
		playerState->DeselectAll();
		GetDocument()->DeleteObjects(toBeCut);
		return true;
	});

	RegisterCommand(kPaste, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState* playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->Paste(*GetDocument());
		return true;
	});

	RegisterCommand(kGroup, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		if (params.GetValue(ModumateParameters::kMake))
		{
			bool combineWithExistingGroups = params.GetValue(TEXT("combine_existing"));
			int32 groupID = GetDocument()->MakeGroupObject(GetWorld(), params.GetValue(kObjectIDs), combineWithExistingGroups, params.GetValue(kParent));
			output.SetValue(kObjectID, groupID);
		}
		else
		{
			GetDocument()->UnmakeGroupObjects(GetWorld(), params.GetValue(kObjectIDs));
		}
		return true;
	});

	RegisterCommand(kReanalyzeGraph, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		UModumateDocument* doc = GetDocument();
		if (doc)
		{
			doc->CalculatePolyhedra();

			TArray<int32> dirtyPlaneIDs;
			for (const AModumateObjectInstance *obj : doc->GetObjectInstances())
			{
				if (obj && (obj->GetObjectType() == EObjectType::OTMetaPlane))
				{
					dirtyPlaneIDs.Add(obj->ID);
				}
			}
			doc->UpdateMitering(GetWorld(), dirtyPlaneIDs);

			return true;
		}
		
		return false;
	});

	RegisterCommand(kCleanAllObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		UModumateDocument* doc = GetDocument();
		if (doc)
		{
			TArray<AModumateObjectInstance *> &allObjects = doc->GetObjectInstances();
			for (AModumateObjectInstance *obj : allObjects)
			{
				obj->MarkDirty(EObjectDirtyFlags::All);
			}

			doc->CleanObjects();

			return true;
		}

		return false;
	});

	RegisterCommand(kReplayDeltas, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerController *playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
		if (!playerController)
		{
			return false;
		}

		playerController->LoadModel(true);
		return true;
	});

	RegisterCommand(kSetFOV, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerController *playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
		AEditModelPlayerPawn *playerPawn = playerController ? Cast<AEditModelPlayerPawn>(playerController->GetPawn()) : nullptr;
		float newFOV = params.GetValue(ModumateParameters::kFieldOfView);

		if (playerPawn && (newFOV > 0.0f))
		{
			return playerPawn->SetCameraFOV(newFOV);
		}

		return false;
	});
}

DECLARE_CYCLE_STAT(TEXT("Process app command queue"), STAT_ModumateProcessCommandQueue, STATGROUP_Modumate)

void UModumateGameInstance::ProcessCommandQueue()
{
	SCOPE_CYCLE_COUNTER(STAT_ModumateProcessCommandQueue);

	static bool reenter = false;
	if (reenter)
	{
		return;
	}
	reenter = true;
	TArray<FString> commands = CommandQueue;
	CommandQueue.Reset();

	for (auto &command : commands)
	{
		if (ConsoleWaitTimer.IsValid())
		{
			CommandQueue.Add(command);
		}
		else
		{
			DoModumateCommand(FModumateCommand::FromJSONString(command));
		}
	}

	reenter = false;
}

void UModumateGameInstance::RegisterCommand(const TCHAR *command, const std::function<bool(const FModumateFunctionParameterSet &, FModumateFunctionParameterSet &)> &fn)
{
	CommandMap.Add(FString(command), new FModumateFunction(fn));
}

FModumateFunctionParameterSet UModumateGameInstance::DoModumateCommand(const FModumateCommand &command)
{
	static bool reenter = false;

	FString commandString = command.GetJSONString();

	// TODO: formalize re-entrancy/yield rules
	if (ConsoleWaitTimer.IsValid() || reenter)
	{
		UE_LOG(LogUnitTest, Display, TEXT("%s"), *commandString);
		CommandQueue.Add(commandString);
		return FModumateFunctionParameterSet();
	}

	reenter = true;

	FModumateFunctionParameterSet params = command.GetParameterSet();

	FModumateFunctionParameterSet fnOutput;

	FString commandName = params.GetValue(FModumateCommand::CommandFieldString);
	auto *fn = CommandMap.Find(commandName);
	if (fn != nullptr)
	{
		bool bSuccess = (*fn)->FN(params, fnOutput);
		fnOutput.SetValue(kSuccess, bSuccess);

		// If we're recording input, then record this command
		AEditModelPlayerController *playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
		if (command.bCaptureForInput && playerController && playerController->InputAutomationComponent &&
			playerController->InputAutomationComponent->IsRecording())
		{
			playerController->InputAutomationComponent->RecordCommand(command.GetJSONString());
		}
	}

	reenter = false;

	if (!ConsoleWaitTimer.IsValid())
	{
		ProcessCommandQueue();
	}

	return fnOutput;
}

void UModumateGameInstance::GetRegisteredCommands(TMap<FString, FString> &OutCommands)
{
	OutCommands.Reset();
	for (auto &kvp : CommandMap)
	{
		// TODO: add support for command descriptions, from parameters or some other source
		OutCommands.Add(kvp.Key, FString());
	}
}

void UModumateGameInstance::Modumate(const FString &Params)
{
	const TCHAR* parsePtr = *Params;

	bool fnSuccess = false;
	FModumateFunctionParameterSet fnOutput;

	FString token;
	if (FParse::Token(parsePtr, token, true))
	{
		FString commandName = token;
		FModumateCommand cmd(token);

		FModumateFunctionParameterSet paramMap;
		while (FParse::Token(parsePtr, token, true))
		{
			static const FString paramDelim(TEXT("="));
			FString paramName, paramToken;
			if (token.Split(paramDelim, &paramName, &paramToken))
			{
				FString paramValue;
				const TCHAR* paramTokenPtr = *paramToken;
				if (FParse::Token(paramTokenPtr, paramValue, true))
				{
					cmd.Param(paramName, paramValue);
				}
			}
		}
		DoModumateCommand(cmd);
	}
}

void UModumateGameInstance::Shutdown()
{
	if (ThumbnailCacheManager)
	{
		ThumbnailCacheManager->Shutdown();
	}

	if (DraftingManager)
	{
		DraftingManager->Shutdown();
	}

	if (DimensionManager)
	{
		DimensionManager->Shutdown();
	}

	if (TooltipManager)
	{
		TooltipManager->Shutdown();
	}

	if (TutorialManager)
	{
		TutorialManager->Shutdown();
	}

	UModumateAnalyticsStatics::ShutdownAnalytics(AnalyticsInstance);

	if (ObjectDatabase)
	{
		ObjectDatabase->Shutdown();
		delete ObjectDatabase;
		ObjectDatabase = nullptr;
	}
}

void UModumateGameInstance::StartGameInstance()
{
	static const FString projectExtension(TEXT("mdmt"));
	static const FString inputLogExtension(TEXT("ilog"));

	// Determine whether there's a file that should be opened as soon as possible, before the command line argument might be interpreted by the default GameInstance.
	bool bUseFile = false;
	IFileManager& fileManger = IFileManager::Get();
	const TCHAR* commandLine = FCommandLine::Get();
	FString potentialFilePath;
	if (FParse::Token(commandLine, potentialFilePath, 0) && fileManger.FileExists(*potentialFilePath))
	{
		bUseFile = true;
		FString filePathPart, fileNamePart, fileExtPart;
		FPaths::Split(potentialFilePath, filePathPart, fileNamePart, fileExtPart);

		if (fileExtPart == projectExtension)
		{
			PendingProjectPath = potentialFilePath;
		}
		else if (fileExtPart == inputLogExtension)
		{
			PendingInputLogPath = potentialFilePath;
		}
		else
		{
			bUseFile = false;
		}

		if (bUseFile)
		{
			UE_LOG(LogTemp, Log, TEXT("Command line specified file: \"%s\""), *PendingProjectPath);
			FCommandLine::Set(TEXT(""));
		}
	}

	// Handle args while at Modumate startup
	if (!bUseFile && ProcessCustomURLArgs(FString(FCommandLine::Get())) && !PendingClientConnectProjectID.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("Command line specified project ID: \"%s\""), *PendingClientConnectProjectID);
		FCommandLine::Set(TEXT(""));
	}

	Super::StartGameInstance();
}

/** Handle setting up encryption keys. Games that override this MUST call the delegate when their own (possibly async) processing is complete. */
void UModumateGameInstance::ReceivedNetworkEncryptionToken(const FString& EncryptionToken, const FOnEncryptionKeyResponse& Delegate)
{
	FString userID, projectID;
	if (!CloudConnection.IsValid() || !FModumateCloudConnection::ParseEncryptionToken(EncryptionToken, userID, projectID))
	{
		FEncryptionKeyResponse response(EEncryptionResponse::InvalidToken,
			FString::Printf(TEXT("Encryption token was invalid, and did not include a User ID + Project ID: %s"), *EncryptionToken));

		Delegate.ExecuteIfBound(response);
	}
	else
	{
		CloudConnection->QueryEncryptionKey(userID, projectID, Delegate);
	}
}

/** Called when a client receives the EncryptionAck control message from the server, will generally enable encryption. */
void UModumateGameInstance::ReceivedNetworkEncryptionAck(const FOnEncryptionKeyResponse& Delegate)
{
	FEncryptionKeyResponse response(EEncryptionResponse::Failure, TEXT("Invalid user!"));

	if (!AccountManager.IsValid() || !CloudConnection.IsValid() || !CloudConnection->IsLoggedIn())
	{
		Delegate.ExecuteIfBound(response);
	}

	const FString& loggedInUserID = AccountManager->GetUserInfo().ID;

	FWorldContext* worldContext = GetWorldContext();
	UPendingNetGame* pendingNetGame = worldContext ? worldContext->PendingNetGame : nullptr;
	FString optionKey = FModumateCloudConnection::EncryptionTokenKey + FString(TEXT("="));
	FString encryptionToken(pendingNetGame ? pendingNetGame->URL.GetOption(*optionKey, TEXT("")) : TEXT(""));

	FString travelUserID, travelProjectID;
	if (!FModumateCloudConnection::ParseEncryptionToken(encryptionToken, travelUserID, travelProjectID) || (travelUserID != loggedInUserID))
	{
		Delegate.ExecuteIfBound(response);
	}

	CloudConnection->QueryEncryptionKey(travelUserID, travelProjectID, Delegate);
}

void UModumateGameInstance::CheckCrashRecovery()
{
	FString recoveryFile = FPaths::Combine(UserSettings.GetLocalTempDir(), kModumateRecoveryFile);
	FString lockFile = FPaths::Combine(UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);

	const FString& recoveryMessage = LOCTEXT("CrashRecoveryMessage", "It looks like Modumate did not shut down cleanly. Would you like to recover your auto-save file?").ToString();
	const FString& recoveryTitle = LOCTEXT("CrashRecoveryTitle", "Recovery").ToString();

	// We have both a lock and a recovery file and the user wants to load it
	// This value is checked in the main menu widget which then decides whether to show the opening menu or go straight to edit level mode
	// Edit level mode checks this value on BeginPlay
	RecoveringFromCrash = FPaths::FileExists(lockFile)
		&& FPaths::FileExists(recoveryFile)
		&& (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *recoveryMessage, *recoveryTitle) == EAppReturnType::Yes);
}

void UModumateGameInstance::Login(const FString& UserName, const FString& Password, const FString& RefreshToken, bool bSaveUserName, bool bSaveRefreshToken)
{
	TWeakPtr<FModumateAccountManager> WeakAMS(AccountManager);
	TWeakObjectPtr<UModumateGameInstance> weakThis(this);
	bool bUsingRefreshToken = (Password.IsEmpty() && !RefreshToken.IsEmpty());

	AccountManager->SetIsFirstLogin(false);
	CloudConnection->Login(UserName, Password, RefreshToken,
		[WeakAMS, weakThis, bSaveUserName, bSaveRefreshToken](bool bSuccessful, const TSharedPtr<FJsonObject>& Response)
		{
			if (bSuccessful)
			{
				TSharedPtr<FModumateAccountManager> SharedAMS = WeakAMS.Pin();
				if (!SharedAMS.IsValid())
				{
					return;
				}

				FModumateUserVerifyParams verifyParams;
				if (FJsonObjectConverter::JsonObjectToUStruct<FModumateUserVerifyParams>(Response.ToSharedRef(), &verifyParams))
				{
					SharedAMS->SetIsFirstLogin(verifyParams.LastDesktopLoginDateTime == 0);
					SharedAMS->SetUserInfo(verifyParams.User);
					SharedAMS->RequestStatus();
				}

				if (weakThis.IsValid())
				{
					weakThis->UserSettings.SavedUserName.Empty();
					weakThis->UserSettings.SavedCredentials.Empty();

					if (bSaveUserName)
					{
						weakThis->UserSettings.SavedUserName = verifyParams.User.Email;

						if (bSaveRefreshToken)
						{
							weakThis->UserSettings.SavedCredentials = verifyParams.RefreshToken;
						}
					}

					weakThis->UserSettings.SaveLocally();
				}
			}
			else
			{
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
					TEXT("Unknown network error"),
					TEXT("Unknown Error"));
			}
		},
		[bUsingRefreshToken](int32 Code, const FString& Error)
		{
			FText messageTitle = LOCTEXT("LoginFailedTitle", "Login Failed");
			if (Code == EHttpResponseCodes::Denied)
			{
				if (bUsingRefreshToken)
				{
					FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
						*LOCTEXT("LoginFailedInvalidToken", "Your saved credentials have expired - please re-enter your password.").ToString(),
						*messageTitle.ToString());
				}
				else
				{
					FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
						*LOCTEXT("LoginFailedInvalidPassword", "Incorrect user name or password.").ToString(),
						*messageTitle.ToString());
				}
			}
			else
			{
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
					*LOCTEXT("LoginFailedConnection", "Cannot reach Modumate - please check your internet or try again in a few minutes.").ToString(),
					*messageTitle.ToString());
			}
		}
	);
}

ELoginStatus UModumateGameInstance::LoginStatus() const
{
	return CloudConnection.IsValid() ? CloudConnection->GetLoginStatus() : ELoginStatus::Disconnected;
}

bool UModumateGameInstance::IsloggedIn() const
{
	return CloudConnection.IsValid() && CloudConnection->IsLoggedIn();
}

bool UModumateGameInstance::ProcessCustomURLArgs(const FString& Args)
{
	UE_LOG(LogTemp, Log, TEXT("ProcessCustomURLArgs: %s"), *Args);

	// Must match the CustomURLArg value in BootstrapPackagedGame.cpp in order to communicate using shared temp files!
	static const FString urlKey(TEXT("-CustomURL="));
	// Must match the NSIS registry key that we use in the installer to register a URL Protocol!
	static const FString urlPrefix(TEXT("mdmt://"));
	static const FString projectPagePrefix(TEXT("projects"));
	static const FString projectPrefix(TEXT("project/"));
	static const FString optionsDelimiter(TEXT("?"));
	static const FString tokenPrefix(TEXT("token="));

	FString parsedURL;
	if (!FParse::Value(*Args, *urlKey, parsedURL))
	{
		return false;
	}

	if (!parsedURL.RemoveFromStart(urlPrefix))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid URL protocol: %s"), *parsedURL);
		return false;
	}

	// Check for options. Currently the only extra option is 'token'
	// If no option, assume parsedURL as projectID
	FString projectString;
	FString optionString;
	bool bHasOptions = parsedURL.Split(optionsDelimiter, &projectString, &optionString);
	if (bHasOptions)
	{
		// Check for RefreshToken
		if (GetCloudConnection() && optionString.RemoveFromStart(tokenPrefix))
		{
			UE_LOG(LogTemp, Log, TEXT("Set PendingToken from URL: %s"), *optionString);
			GetCloudConnection()->SetRefreshToken(optionString);
		}
	}
	else
	{
		projectString = parsedURL;
	}

	// Check if url opens project page
	if (projectString.StartsWith(projectPagePrefix))
	{
		UWorld* world = GetWorld();
		AMainMenuGameMode* mainMenuGameMode = world ? world->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
		if (mainMenuGameMode)
		{
			mainMenuGameMode->OpenProjectPageInWebBrowser();
			return true;
		}
	}

	// Check for projectID
	if (projectString.RemoveFromStart(projectPrefix))
	{
		UE_LOG(LogTemp, Log, TEXT("Set PendingClientConnectProjectID to : %s"), *projectString);
		PendingClientConnectProjectID = projectString;
	}

	return true;
}

void UModumateGameInstance::SlowTick()
{
	UWorld* world = GetWorld();
	if (world == nullptr)
	{
		return;
	}

	// Handle args while Modumate is running
	FString tempMessage;
	if (FModumatePlatform::ConsumeTempMessage(tempMessage))
	{
		ProcessCustomURLArgs(tempMessage);
	}

	// If we received a project ID to open as a message, then either:
	// - Try to close the current edit session to go to the main menu, where once logged in we should auto-open the project
	// - See if we're logged in at the main menu, and try to open the project immediately
	AMainMenuGameMode* mainMenuGameMode = world->GetAuthGameMode<AMainMenuGameMode>();
	if (!PendingClientConnectProjectID.IsEmpty())
	{
		auto* localPlayer = world->GetFirstLocalPlayerFromController();
		auto* editModelController = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(world)) : nullptr;

		if (editModelController)
		{
			BringViewportWindowToFront();
			if (editModelController->CheckSaveModel())
			{
				FString mainMenuMap = UGameMapsSettings::GetGameDefaultMap();
				UGameplayStatics::OpenLevel(this, FName(*mainMenuMap));
			}
		}
		else if (ensure(mainMenuGameMode) && IsloggedIn() && mainMenuGameMode->OpenCloudProject(PendingClientConnectProjectID))
		{
			BringViewportWindowToFront();
			PendingClientConnectProjectID.Empty();
		}
	}

	if (ensure(mainMenuGameMode) && IsloggedIn() && !HasAskedForRestrictedFileUpload)
	{
		HasAskedForRestrictedFileUpload = mainMenuGameMode->UploadRestrictedSaveFile();;
	}

	if (CloudConnection.IsValid())
	{
		CloudConnection->Tick();
	}
}

void UModumateGameInstance::GoToMainMenu(const FText& StatusMessage)
{
	FString mainMenuMap = UGameMapsSettings::GetGameDefaultMap();
	UGameplayStatics::OpenLevel(this, FName(*mainMenuMap));
}

void UModumateGameInstance::OnKickedFromMPSession(const FText& KickReason)
{
	PendingMainMenuStatus = KickReason;
}

void UModumateGameInstance::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorMessage)
{
	PendingMainMenuStatus = LOCTEXT("GenericTravelFailure", "Failed to open project, please try again later.");
}

void UModumateGameInstance::OnNetworkFailure(UWorld* World, UNetDriver* NetDrive, ENetworkFailure::Type FailureType, const FString& ErrorMessage)
{
	PendingMainMenuStatus = LOCTEXT("GenericTravelFailure", "Network error; please check your internet connection and try to reconnect later.");
}

void UModumateGameInstance::OnStartConnectCloudProject(const FString& NewProjectID)
{
	if (!CurClientConnectProjectID.IsEmpty())
	{
		return;
	}

	CurClientConnectProjectID = NewProjectID;

	// TODO: show modal UI here for our project connection status, rather than in a level-specific widget

#if !UE_BUILD_SHIPPING
	// Play a fun sound while connecting to a cloud project
	ProjectConnectionSoundInst = UGameplayStatics::SpawnSound2D(this, ProjectConnectionSound, 1.0f, 1.0f, 0.0f, nullptr, true, true);
	if (ProjectConnectionSoundInst)
	{
		ProjectConnectionSoundInst->Play();
	}
#endif
}

void UModumateGameInstance::OnEndConnectCloudProject()
{
	CurClientConnectProjectID.Empty();

	// TODO: hide modal UI here for our project connection status, rather than in a level-specific widget

	// Stop playing the sound for connecting to a cloud project
	if (ProjectConnectionSoundInst)
	{
		ProjectConnectionSoundInst->Stop();
	}
}

bool UModumateGameInstance::CheckMainMenuStatus(FText& OutStatusMessage)
{
	if (PendingMainMenuStatus.IsEmpty())
	{
		return false;
	}

	OutStatusMessage = MoveTemp(PendingMainMenuStatus);
	return true;
}

void UModumateGameInstance::create_local_project_from_ams()
{
	UWorld* world = GetWorld();
	AMainMenuGameMode* mainMenuGameMode = world ? world->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
	if (mainMenuGameMode)
	{
		mainMenuGameMode->OpenEditModelLevel();
	}
}

void UModumateGameInstance::open_cloud_project_from_ams(int32 ProjectID)
{
	UWorld* world = GetWorld();
	AMainMenuGameMode* mainMenuGameMode = world ? world->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
	if (mainMenuGameMode)
	{
		mainMenuGameMode->OpenCloudProject(FString::FromInt(ProjectID));
	}
}

void UModumateGameInstance::pass_user_package_from_ams(const FString& content)
{
	// Convert JSON content to verifyParams
	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> jsonReader = TJsonReaderFactory<TCHAR>::Create(content);
	if (FJsonSerializer::Deserialize(jsonReader, JsonParsed))
	{
		FModumateUserVerifyParams verifyParams;
		bool bHasMultiplayerFeatureFromAMS = false;

		const TSharedPtr<FJsonValue> jsonRefreshToken = JsonParsed->TryGetField(TEXT("refreshToken"));
		const TSharedPtr<FJsonValue> jsonUserId = JsonParsed->TryGetField(TEXT("userId"));
		const TSharedPtr<FJsonValue> jsonName = JsonParsed->TryGetField(TEXT("name"));
		const TSharedPtr<FJsonValue> jsonEmail = JsonParsed->TryGetField(TEXT("email"));
		const TSharedPtr<FJsonValue> jsonWorkspace = JsonParsed->TryGetField(TEXT("workspace"));
		const TSharedPtr<FJsonValue> jsonFeatures = JsonParsed->TryGetField(TEXT("features"));
		static const FString multiplayerFeatureFlagString(TEXT("multiplayer"));

		if (jsonRefreshToken.IsValid())
		{
			verifyParams.RefreshToken = jsonRefreshToken->AsString();
		}
		if (jsonUserId.IsValid())
		{
			verifyParams.User.ID = jsonUserId->AsString();
		}
		if (jsonName.IsValid())
		{
			verifyParams.User.Firstname = jsonName->AsString();
		}
		if (jsonEmail.IsValid())
		{
			verifyParams.User.Email = jsonEmail->AsString();
		}
		if (jsonWorkspace.IsValid())
		{
			AccountManager->CachedWorkspace = jsonWorkspace->AsString();
		}
		if (jsonFeatures.IsValid())
		{
			TArray<TSharedPtr<FJsonValue>> arrayFeature = jsonFeatures->AsArray();
			for (const TSharedPtr<FJsonValue>& curFeature : arrayFeature)
			{
				if (curFeature->AsString() == multiplayerFeatureFlagString)
				{
					bHasMultiplayerFeatureFromAMS = true;
					break;
				}
			}
		}

		// Save features
		AccountManager->SetHasMultiplayerFeature(bHasMultiplayerFeatureFromAMS);

		// Save verifyParams 
		UserSettings.SavedCredentials = verifyParams.RefreshToken;
		AccountManager->SetUserInfo(verifyParams.User);
		TWeakPtr<FModumateAccountManager> weakAccountManager(AccountManager);

		// TODO: Reviews steps to get AuthToken
		// 1. RequestAuthTokenRefresh() requires a connected status, is pass_refresh_token_from_ams() a valid and sufficient guess to assume user has logged in?
		// 2. RefreshToken is private in CloudConnection, can we use SetRefreshToken() from here?
		CloudConnection->SetLoginStatus(ELoginStatus::Connected);
		CloudConnection->SetRefreshToken(UserSettings.SavedCredentials);

		CloudConnection->RequestAuthTokenRefresh(UserSettings.SavedCredentials, [weakAccountManager](bool bSuccess, const TSharedPtr<FJsonObject>&)
		{
			auto am = weakAccountManager.Pin();
			if (bSuccess && am.IsValid())
			{
				am->RequestStatus();
			}
		},
			[](int32 Code, const FString& Message)
		{
			UE_LOG(LogNet, Error, TEXT("Auth token request via CEF failed - %s (%d)"), *Message, Code);
		});
	}
}

void UModumateGameInstance::open_offline_project_from_ams()
{
	UWorld* world = GetWorld();
	AMainMenuGameMode* mainMenuGameMode = world ? world->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
	if (mainMenuGameMode)
	{
		mainMenuGameMode->OpenOfflineProjectPicker();
	}
}

FString UModumateGameInstance::get_refresh_token_for_ams() const
{
	if (GetCloudConnection())
	{
		return GetCloudConnection()->GetRefreshToken();
	}
	return FString();
}

void UModumateGameInstance::open_url_in_browser_from_ams(const FString& InURL)
{
	FPlatformProcess::LaunchURL(*InURL, nullptr, nullptr);
}

void UModumateGameInstance::on_logout_from_ams()
{
	if (GetCloudConnection())
	{
		GetCloudConnection()->OnLogout();
	}
}

void UModumateGameInstance::BringViewportWindowToFront()
{
	FSceneViewport* sceneViewport;
	UGameViewportClient* gameViewport;
	if (UModumateFunctionLibrary::FindViewports(GetWorld(), sceneViewport, gameViewport))
	{
		UModumateFunctionLibrary::ModifyViewportWindow(sceneViewport, 0, 0, true);
	}
}

#undef LOCTEXT_NAMESPACE
