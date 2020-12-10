// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/ModumateGameInstance.h"

#include "Algo/Transform.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Database/ModumateObjectDatabase.h"
#include "Drafting/APDFLLib.h"
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
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerPawn_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/ThumbnailCacheManager.h"
#include "UnrealClasses/TooltipManager.h"
#include "UI/EditModelUserWidget.h"

using namespace Modumate::Commands;
using namespace Modumate::Parameters;
using namespace Modumate;


const FString UModumateGameInstance::TestScriptRelativePath(TEXT("TestScripts"));

UModumateGameInstance::UModumateGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

FModumateDocument *UModumateGameInstance::GetDocument()
{
	AEditModelGameState_CPP *gameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AEditModelGameState_CPP>() : nullptr;
	if (gameState != nullptr)
	{
		return &gameState->Document;
	}
	ensureAlways(false);
	return nullptr;
}


void UModumateGameInstance::Init()
{
	AnalyticsInstance = UModumateAnalyticsStatics::InitAnalytics();
	AccountManager = MakeShared<FModumateAccountManager>();

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
}

TSharedPtr<FModumateAccountManager> UModumateGameInstance::GetAccountManager() const
{
	return AccountManager;
}

void UModumateGameInstance::RegisterAllCommands()
{
	RegisterCommand(kBIMDebug, [this](const FModumateFunctionParameterSet& params, FModumateFunctionParameterSet& output)
	{
		AEditModelPlayerController_CPP* controller = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
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

	RegisterCommand(kMakeNew, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->OnNewModel();
		GetDocument()->MakeNew(GetWorld());
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

	RegisterCommand(kRunScript, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		FString filePath = FPaths::ProjectDir() / TestScriptRelativePath / params.GetValue(Parameters::kFilename);

		TArray<FString> lines;
		if (FFileHelper::LoadFileToStringArray(lines, *filePath))
		{
			for (auto &l : lines)
			{
				if (l.Len() > 0 && l[0] != TCHAR(';'))
				{
					DoModumateCommand(FModumateCommand::FromJSONString(l));
				}
			}

			return true;
		}

		return false;
	});

	RegisterCommand(kScreenPrint, [](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Black, params.GetValue(Parameters::kText));
		return true;
	});

	RegisterCommand(kUndo, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		FModumateDocument* doc = GetDocument();
		AEditModelPlayerController_CPP* playerController = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState_CPP* playerState = playerController->EMPlayerState;

		if (doc && !doc->IsPreviewingDeltas())
		{
			doc->Undo(GetWorld());
			playerState->ValidateSelectionsAndView();
			return true;
		}

		return false;
	});

	RegisterCommand(kRedo, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		FModumateDocument* doc = GetDocument();
		AEditModelPlayerController_CPP* playerController = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState_CPP* playerState = playerController->EMPlayerState;

		if (doc && !doc->IsPreviewingDeltas())
		{
			doc->Redo(GetWorld());
			playerState->ValidateSelectionsAndView();
			return true;
		}

		return false;
	});

	RegisterCommand(kBeginUndoRedoMacro, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		GetDocument()->BeginUndoRedoMacro();
		return true;
	});

	RegisterCommand(kEndUndoRedoMacro, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		GetDocument()->EndUndoRedoMacro();
		return true;
	});

	RegisterCommand(kMakeScopeBox, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		FModumateDocument *doc = GetDocument();
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
		FModumateObjectInstance *ob = GetDocument()->GetObjectById(params.GetValue(kObjectID));
		bool selected = params.GetValue(Parameters::kSelected);
		if (ob != nullptr)
		{
			AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
			playerState->SetObjectSelected(ob, selected);
			return true;
		}
		return false;
	});

	RegisterCommand(kSelectObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		TArray<int32> obIDs = params.GetValue(Parameters::kObjectIDs);
		bool selected = params.GetValue(Parameters::kSelected);

		for (auto obID : obIDs)
		{
			FModumateObjectInstance *ob = GetDocument()->GetObjectById(obID);
			if (ob != nullptr)
			{
				playerState->SetObjectSelected(ob,selected);
			}
		}
		return true;
	});


	RegisterCommand(kSelectAll, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->SelectAll();
		return true;
	});

	RegisterCommand(kSelectInverse, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->SelectInverse();
		return true;
	});

	RegisterCommand(kDeselectAll, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->DeselectAll();
		return true;
	});

	RegisterCommand(kViewGroupObject, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		int32 objID = params.GetValue(kObjectID);
		FString idString = params.GetValue(kObjectID);
		FModumateObjectInstance *ob = nullptr;

		if (objID > 0)
		{
			ob = GetDocument()->GetObjectById(objID);
		}

		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->SetViewGroupObject(ob);
		return true;
	});

	RegisterCommand(kDeleteObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		AEditModelPlayerController_CPP *playerController = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(playerController->PlayerState);
		GetDocument()->DeleteObjects(params.GetValue(kObjectIDs), true, params.GetValue(kIncludeConnected));
		return true;
	});

	RegisterCommand(kDeleteSelectedObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		AEditModelPlayerController_CPP *playerController = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(playerController->PlayerState);
		TArray<FModumateObjectInstance*> obs = playerState->SelectedObjects.Array();
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

		AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
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
		else if (type.Equals(TEXT("dwg")))
		{
			Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController())->OnCreateDwg();
		}
		return true;
	});

	auto copySelected = [this]()
	{
		AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		playerState->CopySelectedToClipboard(*GetDocument());
	};

	RegisterCommand(kCopySelected, [copySelected, this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		copySelected();
		return true;
	});

	RegisterCommand(kCutSelected, [copySelected, this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		copySelected();
		AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		TArray<FModumateObjectInstance*> toBeCut = playerState->SelectedObjects.Array();
		playerState->DeselectAll();
		GetDocument()->DeleteObjects(toBeCut);
		return true;
	});

	RegisterCommand(kPaste, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
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

	RegisterCommand(kDraft, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {

		FModumateDocument *doc = GetDocument();
		doc->ExportPDF(GetWorld(), *(FPaths::ProjectDir() / TEXT("test.pdf")),FVector::ZeroVector, FVector::ZeroVector);
		return true;
	});

	RegisterCommand(kReanalyzeGraph, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		FModumateDocument *doc = GetDocument();
		if (doc)
		{
			doc->CalculatePolyhedra();

			TArray<int32> dirtyPlaneIDs;
			for (const FModumateObjectInstance *obj : doc->GetObjectInstances())
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
		FModumateDocument *doc = GetDocument();
		if (doc)
		{
			TArray<FModumateObjectInstance *> &allObjects = doc->GetObjectInstances();
			for (FModumateObjectInstance *obj : allObjects)
			{
				obj->MarkDirty(EObjectDirtyFlags::All);
			}

			doc->CleanObjects();

			return true;
		}

		return false;
	});

	RegisterCommand(kLogin, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		Login(params.GetValue(Parameters::kUserName), params.GetValue(Parameters::kPassword));
		return true;
	});

	RegisterCommand(kSetFOV, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerController_CPP *playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
		AEditModelPlayerPawn_CPP *playerPawn = playerController ? Cast<AEditModelPlayerPawn_CPP>(playerController->GetPawn()) : nullptr;
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
		AEditModelPlayerController_CPP *playerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
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

void UModumateGameInstance::Modumate(const FString &params)
{
	const TCHAR* parsePtr = *params;

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

void UModumateGameInstance::Login(const FString &userName, const FString &password)
{
	AccountManager->Login(userName, password);
}

ELoginStatus UModumateGameInstance::LoginStatus() const
{
	return AccountManager->GetLoginStatus();
}

bool UModumateGameInstance::IsloggedIn() const
{
	return AccountManager->IsloggedIn();
}
