// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/ModumateGameInstance.h"

#include "Algo/Transform.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Dom/JsonObject.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/ModumateDraftingView.h"
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
#include "UnrealClasses/ThumbnailCacheManager.h"
#include "UnrealClasses/TooltipManager.h"
#include "Online/ModumateCloudConnection.h"

using namespace Modumate::Commands;
using namespace Modumate::Parameters;
using namespace Modumate;


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
}

TSharedPtr<FModumateCloudConnection> UModumateGameInstance::GetCloudConnection() const
{
	return CloudConnection;
}

TSharedPtr<FModumateAccountManager> UModumateGameInstance::GetAccountManager() const
{
	return AccountManager;
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
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Black, params.GetValue(Parameters::kText));
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

	RegisterCommand(kSelectObject, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AModumateObjectInstance *ob = GetDocument()->GetObjectById(params.GetValue(kObjectID));
		bool selected = params.GetValue(Parameters::kSelected);
		if (ob != nullptr)
		{
			AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
			playerState->SetObjectSelected(ob, selected);
			return true;
		}
		return false;
	});

	RegisterCommand(kSelectObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		TArray<int32> obIDs = params.GetValue(Parameters::kObjectIDs);
		bool selected = params.GetValue(Parameters::kSelected);

		for (auto obID : obIDs)
		{
			AModumateObjectInstance *ob = GetDocument()->GetObjectById(obID);
			if (ob != nullptr)
			{
				playerState->SetObjectSelected(ob,selected);
			}
		}
		return true;
	});


	RegisterCommand(kSelectAll, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->SelectAll();
		return true;
	});

	RegisterCommand(kSelectInverse, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->SelectInverse();
		return true;
	});

	RegisterCommand(kDeselectAll, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->DeselectAll();
		return true;
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

		AEditModelPlayerState* playerState = Cast<AEditModelPlayerState>(GetWorld()->GetFirstPlayerController()->PlayerState);
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
		if (params.GetValue(Parameters::kMake))
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
		float newFOV = params.GetValue(Parameters::kFieldOfView);

		if (playerPawn && (newFOV > 0.0f))
		{
			playerController->EMPlayerState->OnCameraFOVUpdate(newFOV);
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

Modumate::FModumateFunctionParameterSet UModumateGameInstance::DoModumateCommand(const FModumateCommand &command)
{
	static bool reenter = false;

	FString commandString = command.GetJSONString();

	// TODO: formalize re-entrancy/yield rules
	if (ConsoleWaitTimer.IsValid() || reenter)
	{
		UE_LOG(LogUnitTest, Display, TEXT("%s"), *commandString);
		CommandQueue.Add(commandString);
		return Modumate::FModumateFunctionParameterSet();
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
}

void UModumateGameInstance::CheckCrashRecovery()
{
	FString recoveryFile = FPaths::Combine(UserSettings.GetLocalTempDir(), kModumateRecoveryFile);
	FString lockFile = FPaths::Combine(UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);

	// We have both a lock and a recovery file and the user wants to load it
	// This value is checked in the main menu widget which then decides whether to show the opening menu or go straight to edit level mode
	// Edit level mode checks this value on BeginPlay
	RecoveringFromCrash = FPaths::FileExists(lockFile)
		&& FPaths::FileExists(recoveryFile)
		&& (Modumate::PlatformFunctions::ShowMessageBox(
			TEXT("Looks like Modumate did not shut down cleanly. Would you like to recover your auto-save file?"),
			TEXT("Recovery"), Modumate::PlatformFunctions::YesNo) == Modumate::PlatformFunctions::Yes);
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
				Modumate::PlatformFunctions::ShowMessageBox(
					TEXT("Unknown network error"),
					TEXT("Unknown Error"), Modumate::PlatformFunctions::Okay);
			}
		},
		[bUsingRefreshToken](int32 Code, const FString& Error)
		{
			if (bUsingRefreshToken)
			{
				Modumate::PlatformFunctions::ShowMessageBox(
					TEXT("Invalid saved credentials - try re-entering user name and password."),
					TEXT("Login Failed"), Modumate::PlatformFunctions::Okay);
			}
			else
			{
				Modumate::PlatformFunctions::ShowMessageBox(
					TEXT("Incorrect user name or password."),
					TEXT("Login Failed"), Modumate::PlatformFunctions::Okay);
			}
		}
	);
}

ELoginStatus UModumateGameInstance::LoginStatus() const
{
	return CloudConnection->GetLoginStatus();
}

bool UModumateGameInstance::IsloggedIn() const
{
	return CloudConnection->IsLoggedIn();
}
