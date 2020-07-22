// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/ModumateGameInstance.h"

#include "Algo/Transform.h"
#include "BIMKernel/BIMProperties.h"
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

using namespace Modumate::Commands;
using namespace Modumate::Parameters;
using namespace Modumate;


const FString UModumateGameInstance::TestScriptRelativePath(TEXT("TestScripts"));

UModumateGameInstance::UModumateGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

Modumate::FModumateDocument *UModumateGameInstance::GetDocument()
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
	AccountManager = MakeShared<Modumate::FModumateAccountManager>();

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
}

TSharedPtr<Modumate::FModumateAccountManager> UModumateGameInstance::GetAccountManager() const
{
	return AccountManager;
}

void UModumateGameInstance::RegisterAllCommands()
{
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

	RegisterCommand(kApplyObjectDelta, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta());
		delta->FromParameterSet(params);
		TArray<TSharedPtr<FDelta>> deltas({ delta });
		return GetDocument()->ApplyDeltas(deltas, GetWorld());
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
		AEditModelPlayerController_CPP *playerController = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState_CPP *playerState = playerController->EMPlayerState;

		if (playerController->ToolIsInUse())
		{
			playerController->AbortUseTool();
			return true;
		}

		if (playerController->InteractionHandle)
		{
			return true;
		}

		GetDocument()->Undo(GetWorld());
		playerState->ValidateSelectionsAndView();
		return true;
	});

	RegisterCommand(kRedo, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		AEditModelPlayerController_CPP *playerController = Cast<AEditModelPlayerController_CPP>(GetWorld()->GetFirstPlayerController());
		AEditModelPlayerState_CPP *playerState = playerController->EMPlayerState;

		if (playerController->ToolIsInUse())
		{
			playerController->AbortUseTool();
			return true;
		}

		if (playerController->InteractionHandle)
		{
			return true;
		}

		GetDocument()->Redo(GetWorld());
		playerState->ValidateSelectionsAndView();
		return true;
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

	RegisterCommand(kMakeTrim, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		TArray<FVector> points = params.GetValue(kControlPoints);
		TArray<int32> controlIndices = params.GetValue(kIndices);
		FName assemblyKey = params.GetValue(kAssembly);
		bool inverted = params.GetValue(kInverted);
		int32 parentID = params.GetValue(kParent);

		AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
		const FModumateObjectAssembly *assembly = gameState->GetAssemblyByKey_DEPRECATED(EToolMode::VE_TRIM, assemblyKey);

		AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		if (ensureAlways(playerState))
		{
			playerState->DeselectAll();
		}

		if (assembly)
		{
			TArray<int32> idsToDelete;
			int32 newObjectID = GetDocument()->MakePointsObject(GetWorld(), idsToDelete, points, controlIndices, EObjectType::OTTrim, inverted, *assembly, parentID, true);
			output.SetValue(kObjectID, newObjectID);

			return true;
		}
		return false;
	});

	RegisterCommand(kMakeMetaVertex, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		TArray<FVector> points = { params.GetValue(kLocation) };
		TArray<int32> ids;
		int32 parentID = params.GetValue(kParent);
		TArray<int32> newObjIDs;

		bool bSuccess = GetDocument()->MakeMetaObject(GetWorld(), points, ids, EObjectType::OTMetaVertex, parentID, newObjIDs);
		output.SetValue(kObjectIDs, newObjIDs);

		return bSuccess;
	});

	RegisterCommand(kMakeMetaEdge, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		TArray<FVector> points = params.GetValue(kControlPoints);
		TArray<int32> ids = params.GetValue(kObjectIDs);
		int32 parentID = params.GetValue(kParent);
		TArray<int32> newObjIDs;

		bool bSuccess = GetDocument()->MakeMetaObject(GetWorld(), points, ids, EObjectType::OTMetaEdge, parentID, newObjIDs);
		output.SetValue(kObjectIDs, newObjIDs);

		return bSuccess;
	});

	RegisterCommand(kMakeMetaPlane, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		TArray<FVector> points = params.GetValue(kControlPoints);
		TArray<int32> ids = params.GetValue(kObjectIDs);
		int32 parentID = params.GetValue(kParent);
		TArray<int32> newObjIDs;

		bool bSuccess = GetDocument()->MakeMetaObject(GetWorld(), points, ids, EObjectType::OTMetaPlane, parentID, newObjIDs);
		output.SetValue(kObjectIDs, newObjIDs);

		return bSuccess;
	});

	RegisterCommand(kMakeCutPlane, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		FModumateDocument *doc = GetDocument();
		if (doc == nullptr)
		{
			return false;
		}

		TArray<FVector> points = params.GetValue(kControlPoints);
		TArray<int32> newObjIDs;

		bool bSuccess = doc->MakeCutPlaneObject(GetWorld(), points, newObjIDs);
		output.SetValue(kObjectIDs, newObjIDs);

		return bSuccess;
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

	RegisterCommand(kSetGeometry, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {

		int32 id = params.GetValue(kObjectID);
		TArray<FVector> points = params.GetValue(kControlPoints);

		FModumateDocument *doc = GetDocument();
		if (doc)
		{
			FModumateObjectInstance *target = doc->GetObjectById(id);
			if (target)
			{
				// Default to the existing extents, unless the parameter was passed in.
				FVector extents = target->GetExtents();
				if (params.HasValue(kExtents))
				{
					extents = params.GetValue(kExtents);
				}

				return doc->UpdateGeometry(GetWorld(), id, points, extents);
			}
		}
		return false;
	});

	RegisterCommand(kMoveMetaVertices, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {

		TArray<int32> vertexIDs = params.GetValue(kObjectIDs);
		TArray<FVector> vertexPositions = params.GetValue(kControlPoints);

		FModumateDocument *doc = GetDocument();
		if ((doc != nullptr) && (vertexIDs.Num() == vertexPositions.Num()))
		{
			return doc->MoveMetaVertices(GetWorld(), vertexIDs, vertexPositions);
		}
		return false;
	});

	RegisterCommand(kSplit, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {

		int32 id = params.GetValue(kObjectID);
		TArray<FVector> pointsA = params.GetValue(TEXT("pointsA"));
		TArray<FVector> pointsB = params.GetValue(TEXT("pointsB"));
		TArray<int32> indicesA = params.GetValue(TEXT("indicesA"));
		TArray<int32> indicesB = params.GetValue(TEXT("indicesB"));

		FModumateDocument *doc = GetDocument();
		if (doc != nullptr)
		{
			doc->Split(id, pointsA, pointsB, indicesA, indicesB);
			return true;
		}
		return false;
	});

	auto makePortal = [this](EObjectType ot, EToolMode tool, const FModumateFunctionParameterSet &params)
	{
		int32 wallid = params.GetValue(kObjectID);
		FVector2D relativePos = params.GetValue(kLocation);
		FName assemblyKey = params.GetValue(kAssembly);
		FQuat relativeRot = params.GetValue(kQuaternion);
		bool inverted = params.GetValue(kInverted);

		AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
		const FModumateObjectAssembly *assembly = gameState->GetAssemblyByKey_DEPRECATED(tool, assemblyKey);


		if (assembly != nullptr)
		{
			GetDocument()->MakePortalAt(GetWorld(), ot, wallid, relativePos, relativeRot, inverted, *assembly);
			return true;
		}

		return false;
	};

	RegisterCommand(kAddWindow, [this,makePortal](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		return makePortal(EObjectType::OTWindow,EToolMode::VE_WINDOW,params);
	});

	RegisterCommand(kAddDoor, [this, makePortal](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		return makePortal(EObjectType::OTDoor, EToolMode::VE_DOOR, params);
	});

	RegisterCommand(kDumpScript, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		TArray<FString> inputHistory = GetDocument()->GetCommandHistory();
		TArray<FString> outputHistory;
		FString filePath = FPaths::ProjectDir() / TestScriptRelativePath / params.GetValue(kFilename, TEXT("script"));

		bool yield = params.GetValue(TEXT("yield"), true);
		float secs = params.GetValue(kSeconds,0.5f);

		FModumateCommand cmd = FModumateCommand(Commands::kYield).Param(Parameters::kSeconds, secs);
		FString yieldStr = cmd.GetJSONString();

		for (auto &line : GetDocument()->GetCommandHistory())
		{
			outputHistory.Add(line);
			if (yield)
			{
				outputHistory.Add(yieldStr);
			}
		}
		// the dump command itself is removed
		outputHistory.RemoveAt(outputHistory.Num() - 1);
		FFileHelper::SaveStringArrayToFile(outputHistory, *filePath);

		return true;
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
		TArray<FModumateObjectInstance*> obs = playerState->SelectedObjects;
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

	RegisterCommand(kMoveObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {

		GetDocument()->MoveObjects(GetWorld(),params.GetValue(kObjectIDs), params.GetValue(kDelta));
		return true;
	});

	RegisterCommand(kRotateObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {

		GetDocument()->RotateObjects(GetWorld(),params.GetValue(kObjectIDs), params.GetValue(kOrigin), params.GetValue(kQuaternion));
		return true;
	});

	RegisterCommand(kSetObjectTransforms, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		TArray<int32> objectIDs = params.GetValue(kObjectIDs);
		TArray<int32> parentIDs = params.GetValue(kParentIDs);
		TArray<FVector> positions = params.GetValue(kLocation);
		TArray<FQuat> rotations = params.GetValue(kQuaternion);

		bool bSuccess = GetDocument()->SetObjectTransforms(GetWorld(), objectIDs, parentIDs, positions, rotations);

		return bSuccess;
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

	RegisterCommand(kInvertObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		TArray<int32> objIDs = params.GetValue(kObjectIDs);
		TArray<FModumateObjectInstance*> curMOIs;
		for (auto& id : objIDs)
		{
			curMOIs.Add(GetDocument()->GetObjectById(id));
		}
		GetDocument()->InvertObjects(curMOIs);
		return true;
	});

	RegisterCommand(kTransverseObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		AEditModelPlayerState_CPP* playerState = Cast<AEditModelPlayerState_CPP>(GetWorld()->GetFirstPlayerController()->PlayerState);
		TArray<int32> objIDs = params.GetValue(kObjectIDs);
		TArray<FModumateObjectInstance*> curMOIs;
		for (auto& id : objIDs)
		{
			curMOIs.Add(GetDocument()->GetObjectById(id));
		}
		GetDocument()->TransverseObjects(curMOIs);
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
		TArray<FModumateObjectInstance*> toBeCut = playerState->SelectedObjects;
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

	RegisterCommand(kUpdateMOIHoleParams, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output) {
		GetDocument()->AdjustMoiHoleVerts(params.GetValue(kObjectID), params.GetValue(kLocation), params.GetValue(kControlPoints));
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
				for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
				{
					obj->MarkDirty(dirtyFlag);
				}
			}

			doc->CleanObjects();

			return true;
		}

		return false;
	});

	auto getAssemblySpec = [this](const FModumateFunctionParameterSet &Params, BIM::FModumateAssemblyPropertySpec &OutSpec)
	{
		OutSpec.RootPreset = Params.GetValue(Parameters::kPresetKey);

		OutSpec.ObjectType = FindEnumValueByName<EObjectType>(TEXT("EObjectType"), Params.GetValue(kObjectType).AsName());

		Params.ForEachProperty([&OutSpec](const FString &name, const FModumateCommandParameter &param)
		{
			if (name.Contains(TEXT(":")))
			{
				TArray<FString> indexedParam;
				name.ParseIntoArray(indexedParam, TEXT(":"));
				if (ensureAlways(indexedParam.Num() == 2))
				{
					int32 index = FCString::Atoi(*indexedParam[0]);
					if (index == -1)
					{
						OutSpec.RootProperties.SetValue(indexedParam[1], param);
					}
					else
					{
						while (index >= OutSpec.LayerProperties.Num())
						{
							OutSpec.LayerProperties.AddDefaulted();
						}
						OutSpec.LayerProperties[index].SetValue(indexedParam[1], param);
					}
				}
			}
		});
	};

	RegisterCommand(kRemovePresetProjectAssembly, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		// TODO: undo/redoable
		FName assemblyKey = params.GetValue(Parameters::kAssembly);
		ECraftingResult result = GetDocument()->PresetManager.RemoveProjectAssemblyForPreset(params.GetValue(Parameters::kPresetKey));
		return result == ECraftingResult::Success;
	});
	
	RegisterCommand(kUpdateCraftingPreset, [this, getAssemblySpec](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		// TODO: add undo/redoable call in document
		FName outKey;
		ECraftingResult result = GetDocument()->PresetManager.MakeNewOrUpdateExistingPresetFromParameterSet(GetWorld(),params,outKey);
		output.SetValue(Modumate::Parameters::kPresetKey, outKey);
		output.SetValue(Modumate::Parameters::kResult, EnumValueString(ECraftingResult,result));
		return result == ECraftingResult::Success;
	});

	RegisterCommand(kCreateNewAssembly_DEPRECATED, [this, getAssemblySpec](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		int32 version = params.GetValue(FModumateCommand::VersionString);

		if (ensureAlways(version == 3))
		{
			FString tmStr = params.GetValue(kObjectType);
			EObjectType objectType = FindEnumValueByName<EObjectType>(TEXT("EObjectType"), *tmStr);
			EToolMode mode = UModumateTypeStatics::ToolModeFromObjectType(objectType);

			AEditModelGameMode_CPP *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
			BIM::FModumateAssemblyPropertySpec spec;
			FModumateObjectAssembly newAsm;

			EObjectType ot = EnumValueByString(EObjectType, params.GetValue(Parameters::kObjectType).AsString());

			// FFE assemblies are stored directly in the object database and don't receive parameters from the editor
			if (ot == EObjectType::OTFurniture)
			{
				const FModumateObjectAssembly *pAsm = gameMode->ObjectDatabase->PresetManager.GetAssemblyByKey(EToolMode::VE_PLACEOBJECT, params.GetValue(Parameters::kAssembly));
				if (ensureAlways(pAsm != nullptr))
				{
					newAsm = *pAsm;
				}
				else
				{
					return false;
				}
			}
			// All other assemblies have preset parameters
			else
			{
				getAssemblySpec(params, spec);
				if (UModumateObjectAssemblyStatics::DoMakeAssembly(*gameMode->ObjectDatabase, GetDocument()->PresetManager, spec, newAsm) != ECraftingResult::Success)
				{
					return false;
				}
			}

			newAsm = GetDocument()->CreateNewAssembly_DEPRECATED(GetWorld(), UModumateTypeStatics::ToolModeFromObjectType(objectType), newAsm);
			output.SetValue(kAssembly, newAsm.DatabaseKey.ToString());
			return true;
		}
		return false;
	});

	RegisterCommand(kSetAssemblyForObjects, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		TArray<int32> ids = params.GetValue(kObjectIDs);

		FString tmStr = params.GetValue(kToolMode);
		EToolMode toolMode = FindEnumValueByName<EToolMode>(TEXT("EToolMode"), *tmStr);

		FName assemblyKey = FName(*params.GetValue(kAssembly).AsString());
		const FModumateObjectAssembly *pAsm = GetDocument()->PresetManager.GetAssemblyByKey(toolMode, assemblyKey);
		if (ensureAlways(pAsm != nullptr))
		{
			GetDocument()->SetAssemblyForObjects(GetWorld(), ids, *pAsm);
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

	RegisterCommand(kRemoveAssembly_DEPRECATED, [this](const FModumateFunctionParameterSet &params, FModumateFunctionParameterSet &output)
	{
		FName assemblyKey = params.GetValue(Parameters::kAssembly);
		FName replacementKey = params.GetValue(Parameters::kReplacementKey);
		FName toolMode = params.GetValue(Parameters::kToolMode);
		return GetDocument()->RemoveAssembly(GetWorld(), FindEnumValueByName<EToolMode>(TEXT("EToolMode"), toolMode), assemblyKey, replacementKey);
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

	GetDocument()->AddCommandToHistory(commandString);

	FModumateFunctionParameterSet params = command.GetParameterSet();

	FModumateFunctionParameterSet fnOutput;

	FString commandName = params.GetValue(FModumateCommand::CommandFieldString);
	auto *fn = CommandMap.Find(commandName);
	if (ensureAlways(fn != nullptr))
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
