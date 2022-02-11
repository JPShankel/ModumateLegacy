// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelPlayerController.h"

#include "Algo/Accumulate.h"
#include "Algo/Copy.h"
#include "Algo/Transform.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/EditableTextBox.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Engine/SceneCapture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateStats.h"
#include "ModumateCore/ModumateThumbnailHelpers.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateCloudConnection.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelToggleGravityPawn.h"
#include "UnrealClasses/ModumateConsole.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/ModumateObjectInstanceParts.h"
#include "UnrealClasses/ModumateViewportClient.h"
#include "UnrealClasses/AxesActor.h"
#include "UI/AdjustmentHandleWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/DimensionActor.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/DimensionManager.h"
#include "UI/EditModelUserWidget.h"
#include "UI/ModalDialog/ModalDialogWidget.h"
#include "UI/ProjectSystemWidget.h"
#include "UI/TutorialManager.h"
#include "UI/DrawingDesigner/DrawingDesignerWebBrowserWidget.h"
#include "Objects/CutPlane.h"
#include "UI/RightMenu/CutPlaneMenuWidget.h"
#include "Quantities/QuantitiesManager.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "ModumateCore/EnumHelpers.h"


// Tools
#include "ToolsAndAdjustments/Tools/EditModelCabinetTool.h"
#include "ToolsAndAdjustments/Tools/EditModelCopyTool.h"
#include "ToolsAndAdjustments/Tools/EditModelCreateSimilarTool.h"
#include "ToolsAndAdjustments/Tools/EditModelCutPlaneTool.h"
#include "ToolsAndAdjustments/Tools/EditModelDrawingTool.h"
#include "ToolsAndAdjustments/Tools/EditModelFFETool.h"
#include "ToolsAndAdjustments/Tools/EditModelFinishTool.h"
#include "ToolsAndAdjustments/Tools/EditModelGraph2DTool.h"
#include "ToolsAndAdjustments/Tools/EditModelJoinTool.h"
#include "ToolsAndAdjustments/Tools/EditModelLineTool.h"
#include "ToolsAndAdjustments/Tools/EditModelRectangleTool.h"
#include "ToolsAndAdjustments/Tools/EditModelMoveTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPasteTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPortalTools.h"
#include "ToolsAndAdjustments/Tools/EditModelRailTool.h"
#include "ToolsAndAdjustments/Tools/EditModelRoofPerimeterTool.h"
#include "ToolsAndAdjustments/Tools/EditModelRotateTool.h"
#include "ToolsAndAdjustments/Tools/EditModelScopeBoxTool.h"
#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"
#include "ToolsAndAdjustments/Tools/EditModelStairTool.h"
#include "ToolsAndAdjustments/Tools/EditModelStructureLineTool.h"
#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"
#include "ToolsAndAdjustments/Tools/EditModelTrimTool.h"
#include "ToolsAndAdjustments/Tools/EditModelWandTool.h"
#include "ToolsAndAdjustments/Tools/EditModelBackgroundImageTool.h"
#include "ToolsAndAdjustments/Tools/EditModelTerrainTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPointHostedTool.h"
#include "ToolsAndAdjustments/Tools/EditModelGroupTool.h"
#include "ToolsAndAdjustments/Tools/EditModelUngroupTool.h"
#include "ToolsAndAdjustments/Tools/EditModelEdgeHostedTool.h"
#include "ToolsAndAdjustments/Tools/EditModelFaceHostedTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPattern2DTool.h"

const FString AEditModelPlayerController::InputTelemetryDirectory(TEXT("Telemetry"));

#define LOCTEXT_NAMESPACE "ModumateDialog"

/*
* Constructor
*/
AEditModelPlayerController::AEditModelPlayerController()
	: APlayerController()
	, Document(nullptr)
	, SnappingView(nullptr)
	, MaxRayLengthOnHitMiss(1000.0f)
	, UserSnapAutoCreationElapsed(0.0f)
	, UserSnapAutoCreationDelay(0.4f)
	, UserSnapAutoCreationDuration(0.75f)
	, EMPlayerState(nullptr)
	, TestBox(ForceInitToZero)
	, CameraInputLock(false)
	, SelectionMode(ESelectObjectMode::DefaultObjects)
	, MaxRaycastDist(100000.0f)
{
	InputAutomationComponent = CreateDefaultSubobject<UEditModelInputAutomation>(TEXT("InputAutomationComponent"));

	InputHandlerComponent = CreateDefaultSubobject<UEditModelInputHandler>(TEXT("InputHandlerComponent"));
	CameraController = CreateDefaultSubobject<UEditModelCameraController>(TEXT("CameraController"));

	DefaultViewModes = { EEditViewModes::AllObjects, EEditViewModes::Physical, EEditViewModes::MetaGraph, EEditViewModes::Separators, EEditViewModes::SurfaceGraphs };
}

AEditModelPlayerController::~AEditModelPlayerController()
{
	delete SnappingView;
}

void AEditModelPlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Cache our raycasting parameters
	static const FName HandleTraceTag(TEXT("HandleTrace"));
	HandleTraceQueryParams = FCollisionQueryParams(HandleTraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerController), false);
	HandleTraceObjectQueryParams = FCollisionObjectQueryParams(COLLISION_HANDLE);

	// Assign our cached casted AEditModelPlayerState, and its cached casted pointer to us,
	// immediately after it's created.
	// (This is only possible on standalone / server - online clients won't get the PlayerState until replication)
	if (PlayerState != nullptr)
	{
		EMPlayerState = Cast<AEditModelPlayerState>(PlayerState);
		EMPlayerState->EMPlayerController = this;
	}
}

void AEditModelPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	AEditModelPlayerPawn* asEMPlayerPawn = Cast<AEditModelPlayerPawn>(GetPawn());
	if (asEMPlayerPawn)
	{
		EMPlayerPawn = asEMPlayerPawn;
		SetViewTargetWithBlend(EMPlayerPawn);
	}
}

void AEditModelPlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance)
	{
		gameInstance->OnKickedFromMPSession(KickReason);
	}
}

bool AEditModelPlayerController::TryInitPlayerState()
{
	if ((EMPlayerState == nullptr) && PlayerState)
	{
		EMPlayerState = Cast<AEditModelPlayerState>(PlayerState);
	}

	if (bBeganWithPlayerState)
	{
		return true;
	}

	auto* world = GetWorld();
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;

	// Allow initializing player state from within player controller, in case EMPlayerState began first without the controller.
	if (EMPlayerState && EMPlayerState->HasActorBegunPlay() && !EMPlayerState->bBeganWithController && gameInstance)
	{
		EMPlayerState->EMPlayerController = this;

		// Multiplayer clients need to wait until the player state has received a client index, to use for object ID allocation
		if (IsNetMode(NM_Client) && ((EMPlayerState->MultiplayerClientIdx == INDEX_NONE) || (gameState == nullptr)))
		{
			return false;
		}

		if (BeginWithPlayerState() && EMPlayerState->BeginWithController())
		{
			UE_LOG(LogTemp, Log, TEXT("AEditModelPlayerController initialized state and controller."));
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AEditModelPlayerController failed to initialize state and controller!"));
			if (IsNetMode(NM_Client))
			{
				gameInstance->GoToMainMenu(FText::GetEmpty());
			}
		}
	}

	return false;
}

bool AEditModelPlayerController::BeginWithPlayerState()
{
	UWorld* world = GetWorld();
	AEditModelGameState* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;

	if (bBeganWithPlayerState || !(EMPlayerState && gameState && accountManager && cloudConnection && gameInstance->TutorialManager))
	{
		return false;
	}

#if UE_BUILD_SHIPPING
	if (EditModelUserWidget && EditModelUserWidget->ProjectSystemMenu &&
		EditModelUserWidget->ProjectSystemMenu->ButtonSaveProjectAs)
	{
		EditModelUserWidget->ProjectSystemMenu->ButtonSaveProjectAs->SetVisibility(ESlateVisibility::Collapsed);
	}
#endif

	if (IsNetMode(NM_Client))
	{
		ensureMsgf(gameState->Document == nullptr, TEXT("The document should only be missing if we're a client connected to a dedicated server!"));

		FString localUserID = accountManager->GetUserInfo().ID;

		if (ensure(!localUserID.IsEmpty() && (EMPlayerState->MultiplayerClientIdx >= 0) &&
			(EMPlayerState->MultiplayerClientIdx < UModumateDocument::MaxUserIdx)))
		{
			accountManager->SetProjectID(EMPlayerState->CurProjectID);
			gameState->InitDocument(localUserID, EMPlayerState->MultiplayerClientIdx);
		}
		else
		{
			return false;
		}

		// Disable save-as for multiplayer projects
		if (EditModelUserWidget && EditModelUserWidget->ProjectSystemMenu &&
			EditModelUserWidget->ProjectSystemMenu->ButtonSaveProjectAs)
		{
			EditModelUserWidget->ProjectSystemMenu->ButtonSaveProjectAs->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		accountManager->SetProjectID(FString());
	}

	Document = gameState->Document;

	if (EditModelUserWidget && EditModelUserWidget->DrawingDesigner)
	{
		EditModelUserWidget->DrawingDesigner->InitWithController();
	}

	SnappingView = new FModumateSnappingView(Document, this);

	CreateTools();
	SetToolMode(EToolMode::VE_SELECT);

	// If we have a crash recovery, load that 
	// otherwise if the game mode started with a pending project (if it came from the command line, for example) then load it immediately. 

	FString fileLoadPath;
	bool bSetAsCurrentProject = true;
	bool bAddToRecents = true;
	bool bEnableAutoSave = true;

	UGameViewportClient* viewportClient = gameInstance->GetGameViewportClient();
	if (viewportClient)
	{
		viewportClient->OnWindowCloseRequested().BindUObject(this, &AEditModelPlayerController::CheckSaveModel);
		viewportClient->OnToggleFullscreen().AddUObject(this, &AEditModelPlayerController::OnToggleFullscreen);

		static const FName SelectCursorPath(TEXT("NonUAssets/Cursors/Cursor_select_basic"));
		static const FVector2D CursorHotspot(0.28125f, 0.1875f);
		viewportClient->SetHardwareCursor(EMouseCursor::Default, SelectCursorPath, CursorHotspot);

		// Also allow creating a software cursor that can be visible during frame capture
		if (SoftwareCursorWidgetClass && (SoftwareCursorWidget == nullptr))
		{
			SoftwareCursorWidget = GetEditModelHUD()->GetOrCreateWidgetInstance(SoftwareCursorWidgetClass);
			if (ensure(SoftwareCursorWidget))
			{
				viewportClient->AddCursorWidget(EMouseCursor::Default, SoftwareCursorWidget);
			}
		}

		viewportClient->SetUseSoftwareCursorWidgets(false);
	}

	FString recoveryFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateRecoveryFile);

	if (gameInstance->RecoveringFromCrash)
	{
		gameInstance->RecoveringFromCrash = false;

		fileLoadPath = recoveryFile;
		bSetAsCurrentProject = false;
		bAddToRecents = false;
		bEnableAutoSave = true;
	}
	else
	{
		fileLoadPath = gameInstance->PendingProjectPath;
		bSetAsCurrentProject = true;
		bAddToRecents = !gameInstance->TutorialManager->bOpeningTutorialProject;
		bEnableAutoSave = !gameInstance->TutorialManager->bOpeningTutorialProject;
	}

	// Now that we've determined whether there was a pending project, or if it was a tutorial, we can clear the flags.
	bool bOpeningTutorialProject = gameInstance->TutorialManager->bOpeningTutorialProject;
	gameInstance->TutorialManager->bOpeningTutorialProject = false;
	gameInstance->PendingProjectPath.Empty();

	EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	FString lockFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);
	FFileHelper::SaveStringToFile(TEXT("lock"), *lockFile);

	TimeOfLastAutoSave = FDateTime::Now();

#if !UE_SERVER
	GetWorldTimerManager().SetTimer(AutoSaveTimer, this, &AEditModelPlayerController::OnAutoSaveTimer, 1.0f, true, 0.0f);

	// Create icon generator in the farthest corner of the universe
	// TODO: Ideally the scene capture comp should capture itself only, but UE4 lacks that feature, for now...
	FActorSpawnParameters iconGeneratorSpawnParams;
	iconGeneratorSpawnParams.Name = FName(*FString::Printf(TEXT("%s_DynamicIconGenerator"), *GetName()));
	DynamicIconGenerator = GetWorld()->SpawnActor<ADynamicIconGenerator>(DynamicIconGeneratorClass, iconGeneratorSpawnParams);
	if (ensureAlways(DynamicIconGenerator))
	{
		DynamicIconGenerator->SetActorLocation(FVector(-100000.f, -100000.f, -100000.f));
	}

	// Create FBXImportManager
	FActorSpawnParameters datasmithImporterSpawnParams;
	datasmithImporterSpawnParams.Name = FName(*FString::Printf(TEXT("%s_editModelDatasmithImporter"), *GetName()));
	EditModelDatasmithImporter = GetWorld()->SpawnActor<AEditModelDatasmithImporter>(EditModelDatasmithImporterClass, datasmithImporterSpawnParams);
#endif

#if !UE_BUILD_SHIPPING
	// Now that we've registered our commands, register them in the UE4 console's autocompletion list
	UModumateConsole* console = Cast<UModumateConsole>(GEngine->GameViewport ? GEngine->GameViewport->ViewportConsole : nullptr);
	if (console)
	{
		console->BuildRuntimeAutoCompleteList(true);
	}
#endif

#if !UE_SERVER
	// Make a clean telemetry directory, also cleans up on EndPlay 
	FString path = FModumateUserSettings::GetLocalTempDir() / InputTelemetryDirectory;
	IFileManager::Get().DeleteDirectory(*path, false, true);
	IFileManager::Get().MakeDirectory(*path);

	// Defer project loading until we've finished initializing everything else that it might need
	// (such as the EditModelUserWidget for displaying warnings)
	if (!fileLoadPath.IsEmpty())
	{
		LoadModelFilePath(fileLoadPath, bSetAsCurrentProject, bAddToRecents, bEnableAutoSave);
	}
	else
	{
		NewModel(false);

		// If we're a multiplayer client, then we should still need to download the project; try to do so now.
		if (IsNetMode(NM_Client) && EMPlayerState->bPendingClientDownload)
		{
			if (!gameState->DownloadDocument(EMPlayerState->CurProjectID))
			{
				return false;
			}
		}
	}

	// Multiplayer clients do not auto-save; the server auto-saves for them.
	if (IsNetMode(NM_Client))
	{
		bCurProjectAutoSaves = false;
	}

	// Begin a walkthrough if requested from the main menu, then clear out the request.
	if (gameInstance->TutorialManager->FromMainMenuWalkthroughCategory != EModumateWalkthroughCategories::None)
	{
		gameInstance->TutorialManager->BeginWalkthrough(gameInstance->TutorialManager->FromMainMenuWalkthroughCategory);
		gameInstance->TutorialManager->FromMainMenuWalkthroughCategory = EModumateWalkthroughCategories::None;
	}

	if (InputAutomationComponent && InputAutomationComponent->IsRecording())
	{
		InputAutomationComponent->RecordLoadedTutorial(bOpeningTutorialProject);
	}
#endif
	if (IsNetMode(NM_Client))
	{
		EMPlayerState->OnProjectPermissionsChanged().AddUObject(this, &AEditModelPlayerController::ProjectPermissionsChangedHandler);
	}

	RegisterCapability<AModumateVoice>();
	RegisterCapability<AModumateTextChat>();

	bBeganWithPlayerState = true;

	return true;
}

void AEditModelPlayerController::OnDownloadedClientDocument(uint32 DownloadedDocHash)
{
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!ensure(gameInstance))
	{
		return;
	}

	if (!ensure(EMPlayerState && EMPlayerState->bPendingClientDownload &&
		(EMPlayerState->ExpectedDownloadDocHash == DownloadedDocHash)))
	{
		static const FString eventName(TEXT("ErrorClientDocumentHash"));
		UModumateAnalyticsStatics::RecordEventCustomString(this, EModumateAnalyticsCategory::Network, eventName, FString::Printf(TEXT("client: %08x server: %08x"), EMPlayerState->ExpectedDownloadDocHash,DownloadedDocHash));

		gameInstance->GoToMainMenu(LOCTEXT("DownloadHashError", "Error downloading project - please try again later."));
		return;
	}

	// Clear the loading status now that we've finished loading
	if (EditModelUserWidget && EditModelUserWidget->ModalDialogWidgetBP)
	{
		EditModelUserWidget->ModalDialogWidgetBP->CloseModalDialog();
	}

	// Let the game instance know that we're no longer pending connection to the current cloud project
	gameInstance->OnEndConnectCloudProject();

	// Let the server know that we've downloaded the document that we expected, so operations should be able to resume.
	EMPlayerState->OnDownloadedDocument(DownloadedDocHash);
}

void AEditModelPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Create the HUD during our initial BeginPlay, rather than deferring until we have PlayerState,
	// to avoid any delays during multiplayer sessions startup
#if !UE_SERVER
	SessionStartTime = FDateTime::Now();

	auto* playerHUD = GetEditModelHUD();
	if (ensure(playerHUD))
	{
		playerHUD->Initialize();
		HUDDrawWidget = playerHUD->HUDDrawWidget;
	}

	EditModelUserWidget = CreateWidget<UEditModelUserWidget>(this, EditModelUserWidgetClass);
	if (ensureAlways(EditModelUserWidget))
	{
		EditModelUserWidget->AddToViewport(1);

		if (EditModelUserWidget->ProjectSystemMenu)
		{
			EditModelUserWidget->ProjectSystemMenu->OnVisibilityChanged.AddDynamic(this, &AEditModelPlayerController::OnToggledProjectSystemMenu);
		}
	}
#endif

	// Set the loading status if we're starting to load as a multiplayer client
	// TODO: This won't render if we want to immediately do any blocking work (like calling BeginWithPlayerState with a fully-loaded document),
	// and this also has a few frames of delay after the previous MainMenuGameMode's status widget stops rendering.
	// We could instead have a pure-native-Slate loading widget setup with GetMoviePlayer()->SetupLoadingScreen and/or
	// a level-independent non-game-thread modal widget system for this type of loading text.
	if (IsNetMode(NM_Client) && EditModelUserWidget && EditModelUserWidget->ModalDialogWidgetBP)
	{
		EditModelUserWidget->ModalDialogWidgetBP->CreateModalDialog(
			LOCTEXT("ClientLoadingStatusTitle", "STATUS"), 
			LOCTEXT("ClientLoadingStatusText", "Opening online project..."),
			TArray<FModalButtonParam>());
	}

	TryInitPlayerState();
}

bool AEditModelPlayerController::StartTelemetrySession(bool bRecordLoadedDocument)
{
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance->GetAccountManager().Get()->ShouldRecordTelemetry())
	{
		return false;
	}

	EndTelemetrySession();

	TelemetrySessionKey = FGuid::NewGuid();

	if (InputAutomationComponent->BeginRecording())
	{
		TimeOfLastUpload = FDateTime::Now();

		if (bRecordLoadedDocument)
		{
			InputAutomationComponent->RecordLoadedProject(Document->CurrentProjectPath, Document->GetLastSerializedHeader(), Document->GetLastSerializedRecord());
		}

		const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
		if (!ensureAlways(projectSettings != nullptr))
		{
			return false;
		}
		const FString& projectVersion = projectSettings->ProjectVersion;

		TSharedPtr<FModumateCloudConnection> cloudConnection = gameInstance->GetCloudConnection();
		if (cloudConnection.IsValid())
		{
			cloudConnection->CreateReplay(TelemetrySessionKey.ToString(), *projectVersion, [](bool bSuccess, const TSharedPtr<FJsonObject>& Response) {
				UE_LOG(LogTemp, Log, TEXT("Created Successfully"));

			}, [](int32 code, const FString& error) {
				UE_LOG(LogTemp, Error, TEXT("Error: %s"), *error);
			});
		}

		TSharedPtr<FModumateAccountManager> accountManager = gameInstance->GetAccountManager();
		if (accountManager.IsValid())
		{
			InputAutomationComponent->RecordUserData(accountManager->GetUserInfo(), accountManager->GetUserStatus());
		}

		return true;
	}

	return false;
}

bool AEditModelPlayerController::EndTelemetrySession(bool bAsyncUpload)
{
	if (InputAutomationComponent->IsRecording())
	{
		UploadInputTelemetry(bAsyncUpload);
		InputAutomationComponent->EndRecording(false);
	}

	// If we don't have a session key, there's nothing to record
	if (TelemetrySessionKey.IsValid())
	{
		TelemetrySessionKey.Invalidate();
	}

	return true;
}

void AEditModelPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	FString lockFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*lockFile);
	GetWorldTimerManager().ClearTimer(AutoSaveTimer);

	EndTelemetrySession(false);
	auto dimensionManager = gameInstance->DimensionManager;
	dimensionManager->Reset();

	// Clean up input telemetry files
	FString path = FModumateUserSettings::GetLocalTempDir() / InputTelemetryDirectory;
	IFileManager::Get().DeleteDirectory(*path,false,true);

	if (VoiceClient)
	{
		UE_LOG(LogTemp, Log, TEXT("Disconnecting Voice Client"));
		VoiceClient->Disconnect();
		if (IsNetMode(NM_DedicatedServer))
		{
			UE_LOG(LogTemp, Log, TEXT("Destroying Voice Client"));
			VoiceClient->Destroy();
		}
		
	}

#if !UE_SERVER
	FTimespan sessionTime = FDateTime::Now() - SessionStartTime;
	UModumateAnalyticsStatics::RecordSessionDuration(this, sessionTime);
#endif

	Super::EndPlay(EndPlayReason);
}

bool AEditModelPlayerController::GetRequiredViewModes(EToolCategories ToolCategory, TArray<EEditViewModes>& OutViewModes) const
{
	OutViewModes.Reset();

	switch (ToolCategory)
	{
		case EToolCategories::MetaGraph:
			OutViewModes = { EEditViewModes::MetaGraph };
			return true;
		case EToolCategories::Separators:
			OutViewModes = { EEditViewModes::Separators, EEditViewModes::AllObjects };
			return true;
		case EToolCategories::SurfaceGraphs:
			OutViewModes = { EEditViewModes::SurfaceGraphs };
			return true;
		case EToolCategories::Attachments:
			OutViewModes = { EEditViewModes::AllObjects };
			return true;
		default:
		case EToolCategories::Unknown:
			return false;
	}
}

bool AEditModelPlayerController::ToolIsInUse() const
{
	return (CurrentTool && CurrentTool->IsInUse());
}

EToolMode AEditModelPlayerController::GetToolMode()
{
	return CurrentTool ? CurrentTool->GetToolMode() : EToolMode::VE_NONE;
}

void AEditModelPlayerController::SetToolMode(EToolMode NewToolMode)
{
	// Don't do anything if we're trying to set the tool to our current one.
	EToolMode curToolMode = GetToolMode();
	if (curToolMode == NewToolMode)
	{
		return;
	}

	// Don't change to move or rotate tool if nothing is selected
	if ((NewToolMode == EToolMode::VE_MOVEOBJECT || NewToolMode == EToolMode::VE_ROTATE)
		&& EMPlayerState->NumItemsSelected() == 0)
	{
		return;
	}

	// Record how long we used the previous tool
	if (curToolMode != EToolMode::VE_NONE)
	{
		UModumateAnalyticsStatics::RecordToolUsage(this, curToolMode, CurrentToolUseDuration);
	}

	if (CurrentTool)
	{
		if (CurrentTool->IsInUse())
		{
			CurrentTool->AbortUse();
		}
		CurrentTool->Deactivate();
		CurrentToolUseDuration = 0.0f;
	}

	if (InteractionHandle != nullptr)
	{
		InteractionHandle->AbortUse();
		InteractionHandle = nullptr;
	}

	CurrentTool = ModeToTool.FindRef(NewToolMode);
	if (CurrentTool)
	{
		EToolCategories newToolCategory = UModumateTypeStatics::GetToolCategory(CurrentTool->GetToolMode());

		// First allow the current tool to override the required view modes, then try to get them by the tool category.
		ValidViewModes.Reset();
		bool bToolSpecifiesViewModes = CurrentTool->GetRequiredViewModes(ValidViewModes) ||
			GetRequiredViewModes(newToolCategory, ValidViewModes);

		// If specified, then switch to the first reported required view mode as the highest priority.
		if (bToolSpecifiesViewModes && (ValidViewModes.Num() > 0))
		{
			EMPlayerState->SetViewMode(ValidViewModes[0]);
		}
		// Otherwise, use the default view modes and keep the current view mode.
		else
		{
			ValidViewModes = DefaultViewModes;
		}

		// TODO: runtime assemblies to be replaced with presets 
		// meantime this will ensure tools start with a default assembly
		if (!CurrentTool->GetAssemblyGUID().IsValid())
		{
			FBIMAssemblySpec assembly;
			AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
			if (gameState->Document->GetPresetCollection().TryGetDefaultAssemblyForToolMode(NewToolMode, assembly))
			{
				CurrentTool->SetAssemblyGUID(assembly.UniqueKey());
			}
		}
		CurrentTool->Activate();
	}

	UpdateMouseTraceParams();
	EMPlayerState->UpdateObjectVisibilityAndCollision();
	EMPlayerState->PostSelectionChanged();

	OnToolModeChanged.Broadcast();
}

void AEditModelPlayerController::AbortUseTool()
{
	if (InteractionHandle)
	{
		InteractionHandle->AbortUse();
		InteractionHandle = nullptr;
	}
	else if (CurrentTool)
	{
		if (CurrentTool->IsInUse())
		{
			CurrentTool->AbortUse();
		}
		else
		{
			SetToolMode(EToolMode::VE_SELECT);
		}
	}
}

void AEditModelPlayerController::SetToolCreateObjectMode(EToolCreateObjectMode CreateObjectMode)
{
	if (CurrentTool)
	{
		CurrentTool->SetCreateObjectMode(CreateObjectMode);
	}
}

void AEditModelPlayerController::OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// TODO: Currently the widgets set the user's focus to "None" after committing text, which
	// locks the user out of keyboard input.  However, mouse input is still available and clicking
	// resets the state because the correct SViewport is the widget under the cursor.

	// This flag is a temporary fix, it sets the focus back to the SViewport on the next actor tick, 
	// which happens to occur after the focus is cleared to None.  To debug this process, type the command
	// SlateDebugger.Start and open the Output Log.
	bResetFocusToGameViewport = true;

	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	EToolMode curToolMode = GetToolMode();
	double lengthCM = 0.f;
	bool bParseSuccess = false;

	AEditModelGameState* gameState = GetWorld()->GetGameState<AEditModelGameState>();
	
	// Most cases input is in imperial unit, unless is specific handle or tool mode
	if (curToolMode == EToolMode::VE_ROTATE || // Rotate tool uses degree
		(InteractionHandle && !InteractionHandle->HasDistanceTextInput()))
	{
		bParseSuccess = UModumateDimensionStatics::TryParseNumber(Text.ToString(), lengthCM);
	}
	else
	{
		const FDocumentSettings& settings = gameState->Document->GetCurrentSettings();
		auto dimension = UModumateDimensionStatics::StringToSettingsFormattedDimension(Text.ToString(), settings);
		if (dimension.Format != EDimensionFormat::Error)
		{
			bParseSuccess = true;
			lengthCM = dimension.Centimeters;
		}
	}

	const float MAX_DIMENSION = 525600 * 12 * 2.54;

	if (bParseSuccess && (lengthCM != 0.0f) && (lengthCM <= MAX_DIMENSION))
	{
		// First, try to use the player controller's input number handling,
		// in case a user snap point is taking input.
		// TODO: this function is not working well anymore - it may need to be updated
		/*
		if (HandleInputNumber(v))
		{
			return;
		}
		//*/

		if (InteractionHandle)
		{
			if (InteractionHandle->HandleInputNumber(lengthCM))
			{
				InteractionHandle = nullptr;
				return;
			}
		}
		else if (CurrentTool && CurrentTool->IsInUse())
		{
			CurrentTool->HandleInputNumber(lengthCM);
			return;
		}
	}
}

void AEditModelPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputHandlerComponent->SetupBindings();

	if (InputComponent)
	{
		// Bind raw inputs, so that we have better control over input logging and forwarding than we do in Blueprint.
		InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Pressed, this, &AEditModelPlayerController::HandleRawLeftMouseButtonPressed);
		InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Released, this, &AEditModelPlayerController::HandleRawLeftMouseButtonReleased);

		// Add an input to test a crash.
		InputComponent->BindKey(FInputChord(EKeys::Backslash, true, true, true, false), EInputEvent::IE_Pressed, this, &AEditModelPlayerController::DebugCrash);

#if !UE_BUILD_SHIPPING
		// Add a debug command to force clean selected objects (Ctrl+F5)
		InputComponent->BindKey(FInputChord(EKeys::F5, false, true, false, false), EInputEvent::IE_Pressed, this, &AEditModelPlayerController::CleanSelectedObjects);

		// Add debug commands for input recording/playback
		InputComponent->BindKey(FInputChord(EKeys::R, false, true, true, false), EInputEvent::IE_Pressed, InputAutomationComponent, &UEditModelInputAutomation::TryBeginRecording);
		InputComponent->BindKey(FInputChord(EKeys::R, true, true, true, false), EInputEvent::IE_Pressed, InputAutomationComponent, &UEditModelInputAutomation::TryEndRecording);
		InputComponent->BindKey(FInputChord(EKeys::I, false, true, true, false), EInputEvent::IE_Pressed, InputAutomationComponent, &UEditModelInputAutomation::TryBeginPlaybackPrompt);
		InputComponent->BindKey(FInputChord(EKeys::I, true, true, true, false), EInputEvent::IE_Pressed, InputAutomationComponent, &UEditModelInputAutomation::TryEndPlayback);
#endif
	}
}

void AEditModelPlayerController::CreateTools()
{
	RegisterTool(CreateTool<USelectTool>());
	RegisterTool(CreateTool<UFFETool>());
	RegisterTool(CreateTool<UMoveObjectTool>());
	RegisterTool(CreateTool<URotateObjectTool>());
	RegisterTool(CreateTool<UWallTool>());
	RegisterTool(CreateTool<UFloorTool>());
	RegisterTool(CreateTool<UCeilingTool>());
	RegisterTool(CreateTool<UDoorTool>());
	RegisterTool(CreateTool<UWindowTool>());
	RegisterTool(CreateTool<UStairTool>());
	RegisterTool(CreateTool<URailTool>());
	RegisterTool(CreateTool<UCabinetTool>());
	RegisterTool(CreateTool<UWandTool>());
	RegisterTool(CreateTool<UFinishTool>());
	RegisterTool(CreateTool<UCountertopTool>());
	RegisterTool(CreateTool<UTrimTool>());
	RegisterTool(CreateTool<URoofFaceTool>());
	RegisterTool(CreateTool<URoofPerimeterTool>());
	RegisterTool(CreateTool<ULineTool>());
	RegisterTool(CreateTool<URectangleTool>());
	RegisterTool(CreateTool<UCutPlaneTool>());
	RegisterTool(CreateTool<UScopeBoxTool>());
	RegisterTool(CreateTool<UJoinTool>());
	RegisterTool(CreateTool<UCreateSimilarTool>());
	RegisterTool(CreateTool<UCopyTool>());
	RegisterTool(CreateTool<UPasteTool>());
	RegisterTool(CreateTool<UStructureLineTool>());
	RegisterTool(CreateTool<UDrawingTool>());
	RegisterTool(CreateTool<UGraph2DTool>());
	RegisterTool(CreateTool<USurfaceGraphTool>());
	RegisterTool(CreateTool<UPanelTool>());
	RegisterTool(CreateTool<UMullionTool>());
	RegisterTool(CreateTool<UPointHostedTool>());
	RegisterTool(CreateTool<UEdgeHostedTool>());
	RegisterTool(CreateTool<UFaceHostedTool>());
	RegisterTool(CreateTool<UBackgroundImageTool>());
	RegisterTool(CreateTool<UTerrainTool>());
	RegisterTool(CreateTool<UGroupTool>());
	RegisterTool(CreateTool<UUngroupTool>());
	RegisterTool(CreateTool <UEditModelPattern2DTool>());
}

void AEditModelPlayerController::RegisterTool(TScriptInterface<IEditModelToolInterface> NewTool)
{
	if (!ensureAlways(NewTool))
	{
		return;
	}

	EToolMode newToolMode = NewTool->GetToolMode();
	if (!ensureAlways((newToolMode != EToolMode::VE_NONE) && !ModeToTool.Contains(newToolMode)))
	{
		return;
	}

	NewTool->Initialize();
	ModeToTool.Add(newToolMode, NewTool);
}

void AEditModelPlayerController::OnHoverHandleWidget(UAdjustmentHandleWidget* HandleWidget, bool bIsHovered)
{
	if (!(HandleWidget && HandleWidget->HandleActor))
	{
		return;
	}

	// Clear the hover handles of actors that have been destroyed
	for (int32 i = HoverHandleActors.Num() - 1; i >= 0; --i)
	{
		auto curHoverHandleActor = HoverHandleActors[i];
		if ((curHoverHandleActor == nullptr) || curHoverHandleActor->IsActorBeingDestroyed())
		{
			HoverHandleActors.RemoveAt(i);
		}
	}

	// Maintain a stack of hovered handle widgets, so we can easily access the latest hovered one for input purposes,
	// but allow the handle widgets to use their own hover system for individual hover state.
	if (bIsHovered)
	{
		if (ensure(!HoverHandleActors.Contains(HandleWidget->HandleActor)))
		{
			HoverHandleActors.Push(HandleWidget->HandleActor);
		}

		HoverHandleActor = HandleWidget->HandleActor;
	}
	else
	{
		HoverHandleActors.RemoveSingle(HandleWidget->HandleActor);
		HoverHandleActor = (HoverHandleActors.Num() > 0) ? HoverHandleActors.Last() : nullptr;
	}

	if ((HoverHandleActors.Num() > 0) && HoverHandleActor == nullptr)
	{
		ensureMsgf(false, TEXT("Hovering over an invalid hover handle actor!"));
	}
}

void AEditModelPlayerController::OnPressHandleWidget(UAdjustmentHandleWidget* HandleWidget, bool bIsPressed)
{
	if (!(HandleWidget && HandleWidget->HandleActor))
	{
		return;
	}

	if (bIsPressed)
	{
		if (InteractionHandle == HandleWidget->HandleActor)
		{
			// If we clicked on the handle we're already using, then end its use
			InteractionHandle->EndUse();
			InteractionHandle = nullptr;
			return;
		}

		if (InteractionHandle && InteractionHandle->IsInUse())
		{
			// If we clicked on a different handle than the one that's in use, then first abort the previous one
			InteractionHandle->AbortUse();
			InteractionHandle = nullptr;
		}

		if (HoverHandleActor == HandleWidget->HandleActor)
		{
			// If we clicked on the handle that we're hovering over, then set it as the new interaction handle
			if (HoverHandleActor->BeginUse())
			{
				InteractionHandle = HoverHandleActor;
			}
			else if (HoverHandleActor->IsInUse())
			{
				HoverHandleActor->AbortUse();
			}
		}
	}
}

void AEditModelPlayerController::OnLButtonDown()
{
	// TODO: for now, using tools may create dimension strings that are incompatible with those created
	// by the current user snap point, so we'll clear them now. In the future, we want to keep track of
	// all of these dimension strings consistently so that we can cycle between them.
	ClearUserSnapPoints();

	static bool bOldHandleBehavior = false;
	if (bOldHandleBehavior)
	{
		if (InteractionHandle)
		{
			InteractionHandle->EndUse();
			InteractionHandle = nullptr;
			return;
		}

		if (HoverHandleActor)
		{
			if (HoverHandleActor->BeginUse())
			{
				InteractionHandle = HoverHandleActor;
			}
			// If the handle reported failure beginning, but it is in use, then abort it.
			else if (!ensureAlways(!HoverHandleActor->IsInUse()))
			{
				HoverHandleActor->AbortUse();
			}
			return;
		}
	}

	if (CurrentTool)
	{
		if (CurrentTool->IsInUse())
		{
			if (!CurrentTool->EnterNextStage())
			{
				CurrentTool->EndUse();
			}
		}
		else
		{
			CurrentTool->BeginUse();
		}
	}
}

void AEditModelPlayerController::OnLButtonUp()
{
	if (CurrentTool && CurrentTool->IsInUse())
	{
		CurrentTool->HandleMouseUp();
	}
}

AEditModelPlayerHUD* AEditModelPlayerController::GetEditModelHUD() const
{
	return GetHUD<AEditModelPlayerHUD>();
}

/*
Load/Save menu
*/

bool AEditModelPlayerController::SaveModel()
{

	if (IsNetMode(NM_Client))
	{
		if (EMPlayerState)
		{
			EMPlayerState->RequestUpload();

			AEditModelGameState* gameState = GetWorld()->GetGameState<AEditModelGameState>();
			if (gameState)
			{
				CaptureProjectThumbnail();
				EMPlayerState->UploadProjectThumbnail(gameState->Document->CurrentEncodedThumbnail);
			}

		}
		return true;
	}

	if (EMPlayerState->LastFilePath.IsEmpty())
	{
		return SaveModelAs();
	}
	return SaveModelFilePath(EMPlayerState->LastFilePath);
}

bool AEditModelPlayerController::SaveModelAs()
{
	if (!CanShowFileDialog())
	{
		return false;
	}

	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	// Try to capture a thumbnail for the project
	CaptureProjectThumbnail();

	EMPlayerState->ShowingFileDialog = true;

	// Open the save file dialog and actually perform the save
	FString filename;
	bool bSaveSuccess = false;
	if (FModumatePlatform::GetSaveFilename(filename, nullptr, FModumatePlatform::INDEX_MODFILE))
	{
		EMPlayerState->ShowingFileDialog = false;
		bSaveSuccess = SaveModelFilePath(filename);
	}
	EMPlayerState->ShowingFileDialog = false;
	return bSaveSuccess;
}

bool AEditModelPlayerController::SaveModelFilePath(const FString &filepath)
{
	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	bool bShowPath = true;
	FText filePathText = FText::FromString(filepath);

	// Try to capture a thumbnail for the project
	CaptureProjectThumbnail();

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	if (gameState->Document->SaveFile(GetWorld(), filepath, true))
	{
		FString saveTitleString = LOCTEXT("SaveSuccessTitle", "Save Project").ToString();
		FText saveMessageFormat = bShowPath ? LOCTEXT("SaveSuccessWithPath", "Saved as {0}") : LOCTEXT("SaveSuccessWithoutPath", "Saved successfully.");
		FString saveMessageString = FText::Format(saveMessageFormat, filePathText).ToString();

		EMPlayerState->LastFilePath = filepath;

		// TODO: replace with our own modal dialog box
		ShowMessageBox(EAppMsgType::Ok, *saveMessageString, *saveTitleString);

		static const FString eventName(TEXT("SaveDocument"));
		UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Session, eventName);
		return true;
	}
	else
	{
		FString saveTitleString = LOCTEXT("SaveFailureTitle", "Error!").ToString();
		FText saveMessageFormat = bShowPath ? LOCTEXT("SaveFailureWithPath", "Could not save as {0}") : LOCTEXT("SaveFailureWithoutPath", "Could not save project.");
		FString saveMessageString = FText::Format(saveMessageFormat, filePathText).ToString();

		// TODO: replace with our own modal dialog box
		ShowMessageBox(EAppMsgType::Ok, *saveMessageString, *saveTitleString);
		return false;
	}
}

bool AEditModelPlayerController::LoadModel(bool bLoadOnlyDeltas)
{
	if (!CanShowFileDialog())
	{
		return false;
	}

	// End the entire telemetry session if we're about to load, since this will either:
	// - end the session early if the user canceled, or
	// - restart the session as soon as the user confirms loading a different document
	// TODO: if we want to avoid ending early in the event that the user cancels the load, we need to capture dialog state.
	EndTelemetrySession();

	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	if (!CheckSaveModel())
	{
		return false;
	}

	EMPlayerState->ShowingFileDialog = true;

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();

	FString filename;
	bool bLoadSuccess = false;

	if (FModumatePlatform::GetOpenFilename(filename, nullptr))
	{
		bLoadSuccess = LoadModelFilePath(filename, true, true, true, bLoadOnlyDeltas);
	}

	EMPlayerState->ShowingFileDialog = false;
	return bLoadSuccess;
}

bool AEditModelPlayerController::LoadModelFilePath(const FString &filename, bool bSetAsCurrentProject, bool bAddToRecents, bool bEnableAutoSave, bool bLoadOnlyDeltas)
{
	EndTelemetrySession();
	EMPlayerState->OnNewModel();
	FString newFilePath = filename;

	bool bLoadSuccess = bLoadOnlyDeltas ? 
		Document->LoadDeltas(GetWorld(), filename, bSetAsCurrentProject, bAddToRecents) :
		Document->LoadFile(GetWorld(), filename, bSetAsCurrentProject, bAddToRecents);

	if (bLoadSuccess)
	{
		static const FString LoadDocumentEventName(TEXT("LoadDocument"));
		UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Session, LoadDocumentEventName);

		StartTelemetrySession(true);

		TimeOfLastAutoSave = FDateTime::Now();
		bWantAutoSave = false;
		bCurProjectAutoSaves = bEnableAutoSave;
	}

	if (bLoadSuccess && bSetAsCurrentProject)
	{
		EMPlayerState->LastFilePath = newFilePath;
	}
	else
	{
		EMPlayerState->LastFilePath.Empty();
	}

	return bLoadSuccess;
}

bool AEditModelPlayerController::CheckSaveModel()
{
	// Don't prompt for saving if we're connected as a dedicated multiplayer client; the server is in charge of auto-saving
	if (!IsNetMode(NM_Client) && Document->IsDirty(true))
	{
		const FString& saveConfirmationText = LOCTEXT("SaveConfirmationMessage", "Save current model?").ToString();
		const FString& saveConfirmationTitle = LOCTEXT("SaveConfirmationTitle", "Save").ToString();

		// TODO: replace with our own modal dialog box
		auto resp = ShowMessageBox(EAppMsgType::YesNoCancel, *saveConfirmationText, *saveConfirmationTitle);
		if (resp == EAppReturnType::Yes)
		{
			if (!SaveModel())
			{
				return false;
			}
		}
		if (resp == EAppReturnType::Cancel)
		{
			return false;
		}
	}
	return true;
}

void AEditModelPlayerController::NewModel(bool bShouldCheckForSave)
{
	if (!CanShowFileDialog())
	{
		return;
	}

	if (bShouldCheckForSave)
	{
		if (!CheckSaveModel())
		{
			return;
		}
	}

	EndTelemetrySession();

	static const FString NewDocumentEventName(TEXT("NewDocument"));
	UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Session, NewDocumentEventName);

	EMPlayerState->OnNewModel();
	Document->MakeNew(GetWorld());

	TimeOfLastAutoSave = FDateTime::Now();
	bWantAutoSave = false;
	bCurProjectAutoSaves = true;

	// If we're starting with an input log that we want to load, then try to play it back now.
	bool bPlayingBackInput = false;
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (ensure(gameInstance && InputAutomationComponent) && !gameInstance->PendingInputLogPath.IsEmpty())
	{
		bPlayingBackInput = InputAutomationComponent->BeginPlayback(gameInstance->PendingInputLogPath, true, 1.0f, true);
	}

	if (bPlayingBackInput)
	{
		gameInstance->PendingInputLogPath.Empty();
	}
	else
	{
		StartTelemetrySession(false);
	}
}

void AEditModelPlayerController::SetThumbnailCapturer(ASceneCapture2D* InThumbnailCapturer)
{
	ThumbnailCapturer = InThumbnailCapturer;
	if (ThumbnailCapturer)
	{
		USceneCaptureComponent2D *captureComp = ThumbnailCapturer->GetCaptureComponent2D();
		if (captureComp && (captureComp->TextureTarget == nullptr))
		{
			const FName targetName = MakeUniqueObjectName(captureComp, UTextureRenderTarget2D::StaticClass(), TEXT("SceneCaptureTextureTarget"));
			captureComp->TextureTarget = NewObject<UTextureRenderTarget2D>(captureComp, targetName);
			const FIntPoint &defaultThumbSize = FModumateThumbnailHelpers::DefaultThumbSize;
			captureComp->TextureTarget->InitCustomFormat(defaultThumbSize.X, defaultThumbSize.Y, PF_B8G8R8A8, false);
		}
	}
}

bool AEditModelPlayerController::CaptureProjectThumbnail()
{
	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	USceneCaptureComponent2D *captureComp = ThumbnailCapturer ? ThumbnailCapturer->GetCaptureComponent2D() : nullptr;
	if (gameState && captureComp && captureComp->TextureTarget)
	{
		// First, position the thumbnail capturer in a position that can see the whole project
		float captureAspect = (float)captureComp->TextureTarget->SizeX / (float)captureComp->TextureTarget->SizeY;
		float captureMaxAspect = FMath::Max(captureAspect, 1.0f / captureAspect);

		FSphere projectBounds = gameState->Document->CalculateProjectBounds().GetSphere();

		auto captureVector = (FVector::LeftVector.RotateAngleAxis(ThumbnailCaptureRotationZ, FVector::UpVector).RotateAngleAxis(ThumbnailCaptureRotationY, FVector::ForwardVector));
		captureVector.Normalize();

		FVector captureOrigin = CalculateViewLocationForSphere(projectBounds, captureVector, captureMaxAspect, ThumbnailCaptureFOV);
		FRotator captureRotation = UKismetMathLibrary::FindLookAtRotation(captureOrigin, projectBounds.Center);

		captureComp->GetOwner()->SetActorLocation(captureOrigin);
		captureComp->GetOwner()->SetActorRotation(captureRotation);
		
		// Perform the actual scene capture
		captureComp->CaptureScene();

		// Now, read the render texture contents, compress it, and encode it as a string for the project metadata.
		FString encodedThumbnail;
		if (gameState && FModumateThumbnailHelpers::CreateProjectThumbnail(captureComp->TextureTarget, encodedThumbnail))
		{
			gameState->Document->CurrentEncodedThumbnail = encodedThumbnail;
			return true;
		}
	}

	return false;
}

bool AEditModelPlayerController::GetScreenshotFileNameWithDialog(FString& Filepath, FString& Filename)
{
	if (!CanShowFileDialog())
	{
		return false;
	}

	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance)
	{
		return false;
	}

	auto cloudConnection = gameInstance->GetCloudConnection();
	UWorld* world = GetWorld();
	TFunction<bool()>  networkTickCall = [cloudConnection, world]() { cloudConnection->NetworkTick(world); return true; };

	// Open the file dialog
	bool bChoseFile = false;
	FString fullFilePath;
	EMPlayerState->ShowingFileDialog = true;
	if (FModumatePlatform::GetSaveFilename(fullFilePath, networkTickCall, FModumatePlatform::INDEX_PNGFILE))
	{
		bChoseFile = true;
	}

	// Parse file path
	FString nameString, extensionString;
	FPaths::Split(fullFilePath, Filepath, nameString, extensionString);
	Filename = FPaths::SetExtension(nameString, extensionString);

	EMPlayerState->ShowingFileDialog = false;

	return bChoseFile;
}

void AEditModelPlayerController::LaunchCloudWorkspacePlanURL()
{
	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (cloudConnection.IsValid())
	{
		FPlatformProcess::LaunchURL(*cloudConnection->GetCloudWorkspacePlansURL(), nullptr, nullptr);
	}
}

bool AEditModelPlayerController::MoveToParentGroup()
{
	if (Document)
	{
		const auto* groupObject = Document->GetObjectById(Document->GetActiveVolumeGraphID());
		const auto* groupParent = groupObject ? groupObject->GetParentObject() : nullptr;
		if (groupParent)
		{
			TArray<int32> affectedGroups;
			ensure(UModumateObjectStatics::GetGroupIdsForGroupChange(Document, groupParent->ID, affectedGroups));
			Document->SetActiveVolumeGraphID(groupParent->ID);
			EMPlayerState->PostGroupChanged(affectedGroups);
			return true;
		}
	}
	return false;
}

bool AEditModelPlayerController::OnCreateDwg()
{
	if (!CanShowFileDialog())
	{
		return false;
	}

	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	bool retValue = true;

	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance)
	{
		return false;
	}

	static const FText dialogTitle = LOCTEXT("DWGCreationText", "DWG Creation");
	if (!gameInstance->IsloggedIn())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(FString(TEXT("You must be logged in to export to DWG files")) ),
			&dialogTitle);
		return false;
	}

	EMPlayerState->ShowingFileDialog = true;

	FString filename;

	auto cloudConnection = gameInstance->GetCloudConnection();
	UWorld* world = GetWorld();
	TFunction<bool()>  networkTickCall = [cloudConnection, world]() { cloudConnection->NetworkTick(world); return true; };

	if (FModumatePlatform::GetSaveFilename(filename, networkTickCall, FModumatePlatform::INDEX_DWGFILE))
	{
		EMPlayerState->ShowingFileDialog = false;

		if (!Document->ExportDWG(GetWorld(), *filename))
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DWGCreationFailText", "DWG Creation Failed"),
				&dialogTitle);
			retValue = false;
		}

	}
	else
	{
		EMPlayerState->ShowingFileDialog = false;
		retValue = false;
	}

	return retValue;
}

bool AEditModelPlayerController::OnCreateQuantitiesCsv(const TFunction<void(FString, bool)>& UsageNotificationCallback)
{
	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	bool retValue = true;

	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance)
	{
		return false;
	}

	auto cloudConnection = gameInstance->GetCloudConnection();
	UWorld* world = GetWorld();
	TFunction<bool()>  networkTickCall = [cloudConnection, world]() { cloudConnection->NetworkTick(world); return true; };

	FString filename;
	EMPlayerState->ShowingFileDialog = true;
	auto quantitiesManager = gameInstance->GetQuantitiesManager();
	if (FModumatePlatform::GetSaveFilename(filename, networkTickCall, FModumatePlatform::INDEX_CSVFILE))
	{
		EMPlayerState->ShowingFileDialog = false;
		FModalButtonParam dismissButton(EModalButtonStyle::Default, LOCTEXT("SavedCSVok", "OK"), nullptr);

		if (!quantitiesManager->CreateReport(filename))
		{
			if (EditModelUserWidget && EditModelUserWidget->ModalDialogWidgetBP)
			{
				EditModelUserWidget->ModalDialogWidgetBP->CreateModalDialog(
					LOCTEXT("ClientExportQuantitiesError", "Error"),
					LOCTEXT("QuantityEstimateCreateFail", "Quantity Estimate CSV creation failed"),
					TArray<FModalButtonParam>({ dismissButton }));
			}

			retValue = false;
		}
		else
		{
			gameInstance->GetAccountManager()->NotifyServiceUse(FModumateAccountManager::ServiceQuantityEstimates, UsageNotificationCallback);
			if (EditModelUserWidget && EditModelUserWidget->ModalDialogWidgetBP)
			{
				EditModelUserWidget->ModalDialogWidgetBP->CreateModalDialog(
					LOCTEXT("ClientExportQuantities", "Notice"),
					FText::Format(LOCTEXT("ClientExportQuantitiesText", "Saved Quantity Estimates to {0}"), FText::FromString(filename)),
					TArray<FModalButtonParam>({ dismissButton }));
			}
		}
	}
	else
	{
		EMPlayerState->ShowingFileDialog = false;
	}

	return retValue;
}

bool AEditModelPlayerController::TakeScreenshot()
{
	// Get path from dialog
	FString filePath, fileName;
	if (!GetScreenshotFileNameWithDialog(filePath, fileName))
	{
		return false;
	}

	// Set screenshot taker to the same condition as the current camera
	APlayerCameraManager* cameraManager = this->PlayerCameraManager;
	if (ensureAlways(cameraManager && EMPlayerPawn && EMPlayerPawn->ScreenshotTaker))
	{
		EMPlayerPawn->ScreenshotTaker->SetWorldTransform(cameraManager->GetActorTransform());
		EMPlayerPawn->ScreenshotTaker->FOVAngle = cameraManager->GetFOVAngle();
	}
	else
	{
		return false;
	}

	// Create render target
	// Support customize screenshot res?
	int32 screenshotWidth = 1920;
	int32 screenshotHeight = 1080;
	UTextureRenderTarget2D* screenshotRT = UKismetRenderingLibrary::CreateRenderTarget2D(this, screenshotWidth, screenshotHeight, ETextureRenderTargetFormat::RTF_RGBA8_SRGB);
	EMPlayerPawn->ScreenshotTaker->TextureTarget = screenshotRT;

	// Hide axes
	if (ensureAlways(AxesActor))
	{
		AxesActor->SetActorHiddenInGame(true);
	}

	// Capture and export
	EMPlayerPawn->ScreenshotTaker->CaptureScene();
	UKismetRenderingLibrary::ExportRenderTarget(this, screenshotRT, filePath, fileName);

	// Unhide axes
	if (AxesActor)
	{
		AxesActor->SetActorHiddenInGame(false);
	}

	return true;
}

void AEditModelPlayerController::DeleteActionDefault()
{
	ModumateCommand(
		FModumateCommand(ModumateCommands::kDeleteSelectedObjects)
		.Param(ModumateParameters::kIncludeConnected, true)
	);
}

void AEditModelPlayerController::DeleteActionOnlySelected()
{
	ModumateCommand(
		FModumateCommand(ModumateCommands::kDeleteSelectedObjects)
		.Param(ModumateParameters::kIncludeConnected, false)
	);
}

bool AEditModelPlayerController::HandleInputNumber(double inputNumber)
{
	// This input number handler is intended as a fallback, when the player state's tools do not handle it.
	// TODO: reconcile the back-and-forth input handling between PlayerController and PlayerState.

	// If there's an active tool or handle that can receive the user snap point input, then use it here.

	AAdjustmentHandleActor *curHandle = InteractionHandle;

	bool bUseHandle = (curHandle && curHandle->IsInUse());
	bool bUseTool = (!bUseHandle && CurrentTool && CurrentTool->IsActive());

	FTransform activeUserSnapPoint;
	FVector cursorPosFlat, cursorHeightDelta;
	if ((bUseTool || bUseHandle) && GetActiveUserSnapPoint(activeUserSnapPoint) &&
		GetCursorFromUserSnapPoint(activeUserSnapPoint, cursorPosFlat, cursorHeightDelta))
	{
		// Calculate the new position of the cursor and tool use by projecting from the user snap point by the input number.
		FVector activeUserSnapPos = activeUserSnapPoint.GetLocation();
		FVector dirToSnappedCursor = (cursorPosFlat - activeUserSnapPos).GetSafeNormal();
		FVector projectedInputPoint = activeUserSnapPos + dirToSnappedCursor * inputNumber;

		FVector2D projectedCursorScreenPos;
		if (ProjectWorldLocationToScreen(projectedInputPoint, projectedCursorScreenPos))
		{
			SetMouseLocation(projectedCursorScreenPos.X, projectedCursorScreenPos.Y);
			ClearUserSnapPoints();

			EMPlayerState->SnappedCursor.WorldPosition = projectedInputPoint;

			if (bUseHandle)
			{
				curHandle->EndUse();
				return true;
			}
			else if (bUseTool)
			{
				if (CurrentTool->IsInUse())
				{
					return CurrentTool->EnterNextStage();
				}
				else
				{
					return CurrentTool->BeginUse();
				}
			}
		}
	}

	return false;
}

bool AEditModelPlayerController::HandleInvert()
{
	if (InteractionHandle != nullptr)
	{
		InteractionHandle->HandleInvert();
		return true;
	}
	else if (CurrentTool && CurrentTool->HandleInvert())
	{
		return true;
	}

	return false;
}

bool AEditModelPlayerController::HandleEscapeKey()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::HandleEscapeKey"));

	ClearUserSnapPoints();
	EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	EditModelUserWidget->ToggleBIMDesigner(false);

	if (GetPawn<AEditModelToggleGravityPawn>())
	{
		ToggleGravityPawn();
	}

	if (HoverHandleActor != nullptr)
	{
		HoverHandleActor = nullptr;
	}
	if (InteractionHandle != nullptr)
	{
		InteractionHandle->AbortUse();
		InteractionHandle = nullptr;
		return true;
	}
	else if (CurrentTool && CurrentTool->IsInUse())
	{
		CurrentTool->AbortUse();
		return true;
	}
	else if (CurrentTool && (CurrentTool->GetToolMode() != EToolMode::VE_SELECT))
	{
		SetToolMode(EToolMode::VE_SELECT);
		EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		return true;
	}
	else if (EMPlayerState->SelectedObjects.Num() + EMPlayerState->SelectedGroupObjects.Num() > 0)
	{
		DeselectAll();
		return true;
	}
	else if (EMPlayerState->ViewGroupObject)  // TODO: remove
	{
		SetViewGroupObject(EMPlayerState->ViewGroupObject->GetParentObject());
	}
	else if(EditModelUserWidget->EMUserWidgetHandleEscapeKey())
	{
		return true;
	}
	else if (MoveToParentGroup())
	{
		return true;
	}

	return false;
}

bool AEditModelPlayerController::IsShiftDown() const
{
	return IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
}

bool AEditModelPlayerController::IsControlDown() const
{
	return IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl) ||
		IsInputKeyDown(EKeys::LeftCommand) || IsInputKeyDown(EKeys::RightCommand);
}

void AEditModelPlayerController::OnAutoSaveTimer()
{
#if !UE_SERVER
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());

	FTimespan ts(FDateTime::Now().GetTicks() - TimeOfLastAutoSave.GetTicks());

	if ((ts.GetTotalSeconds() > gameInstance->UserSettings.AutoBackupFrequencySeconds) &&
		gameInstance->UserSettings.AutoBackup && bCurProjectAutoSaves &&
		Document && Document->IsDirty(false))
	{
		bWantAutoSave = true;
	}

	ts = FTimespan(FDateTime::Now().GetTicks() - TimeOfLastUpload.GetTicks());
	if ((ts.GetTotalSeconds() > gameInstance->UserSettings.TelemetryUploadFrequencySeconds) &&
		gameInstance->GetAccountManager().Get()->ShouldRecordTelemetry())
	{
		bWantTelemetryUpload = true;
	}
#endif
}

DECLARE_CYCLE_STAT(TEXT("Edit tick"), STAT_ModumateEditTick, STATGROUP_Modumate)

bool AEditModelPlayerController::UploadInputTelemetry(bool bAsynchronous) const
{
	if (!InputAutomationComponent->IsRecording())
	{
		return false;
	}

	FString path = FModumateUserSettings::GetLocalTempDir() / InputTelemetryDirectory;
	FString cacheFile = path / TelemetrySessionKey.ToString() + FEditModelInputLog::LogExtension;

	double ilogStartTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Log, TEXT("Telemetry: saving & uploading %s..."), *cacheFile);

	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	TSharedPtr<FModumateCloudConnection> cloud = gameInstance->GetCloudConnection();

	TFuture<bool> future = Async(EAsyncExecution::ThreadPool,
		[sessionKey = TelemetrySessionKey, cacheFile, cloud, ilogStartTime, saveTask = InputAutomationComponent->MakeSaveLogTask(cacheFile)]()
		{
			if (saveTask() && cloud.IsValid())
			{
				cloud->UploadReplay(sessionKey.ToString(), *cacheFile,
					[ilogStartTime, cacheFile](bool bSuccess, const TSharedPtr<FJsonObject>& Response)
					{
						int32 saveUploadDurationMS = FMath::RoundToInt((FPlatformTime::Seconds() - ilogStartTime) * 1000.0);
						int64 fileSizeKB = IFileManager::Get().FileSize(*cacheFile) / 1024;
						UE_LOG(LogTemp, Log, TEXT("Telemetry: Upload success after %dms (%lldkB)"), saveUploadDurationMS, fileSizeKB);
					},
					[](int32 code, const FString& error)
					{
						UE_LOG(LogTemp, Error, TEXT("Telemetry: Upload error: %s"), *error);
					}
				);
			}
			return true;
		}
	);

	if (bAsynchronous)
	{
		return true;
	}
	else
	{
		future.Wait();
		return future.Get();
	}
}

void AEditModelPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SCOPE_CYCLE_COUNTER(STAT_ModumateEditTick);

	// Allow the player state initialization to happen after BeginPlay, in case EditModelPlayerState hadn't begun during our BeginPlay,
	// or if we're a multiplayer client waiting for UI before fully initializing.
	TryInitPlayerState();

	// Postpone any meaningful work until we have a valid PlayerState
	if (!bBeganWithPlayerState || !ensure(EMPlayerState))
	{
		return;
	}

	if (bResetFocusToGameViewport)
	{
		bResetFocusToGameViewport = false;
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	// Don't perform hitchy functions while tools or handles are in use
	if (InteractionHandle == nullptr && !ToolIsInUse())
	{
		if (bWantTelemetryUpload)
		{
			TimeOfLastUpload = FDateTime::Now();
			bWantTelemetryUpload = false;
			if (TelemetrySessionKey.IsValid())
			{
				UploadInputTelemetry(true);
			}
		}

		if (bWantAutoSave)
		{
			UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
			FString tempDir = gameInstance->UserSettings.GetLocalTempDir();
			FString oldFile = FPaths::Combine(tempDir, kModumateRecoveryFileBackup);
			FString newFile = FPaths::Combine(tempDir, kModumateRecoveryFile);

			//Look for the first available (of 3) backup files and delete the one following it
			FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*oldFile);
			FPlatformFileManager::Get().GetPlatformFile().MoveFile(*oldFile, *newFile);
			AEditModelGameState* gameState = GetWorld()->GetGameState<AEditModelGameState>();
			gameState->Document->SaveFile(GetWorld(), newFile, false, true);

			TimeOfLastAutoSave = FDateTime::Now();
			bWantAutoSave = false;
		}
	}

	// Keep track of how long we've been using the current tool
	if (CurrentTool != nullptr)
	{
		CurrentToolUseDuration += DeltaTime;
	}

	TickInput(DeltaTime);

	if (EMPlayerState->bShowDocumentDebug)
	{
		Document->DisplayDebugInfo(GetWorld());

		for (auto& sob : EMPlayerState->SelectedObjects)
		{
			FVector p = sob->GetLocation();
			GetWorld()->LineBatcher->DrawLine(p - FVector(20, 0, 0), p + FVector(20, 0, 0), FColor::Blue, SDPG_MAX, 2, 0.0);
			GetWorld()->LineBatcher->DrawLine(p - FVector(0, 20, 0), p + FVector(0, 20, 0), FColor::Blue, SDPG_MAX, 2, 0.0);

			FVector d = sob->GetRotation().RotateVector(FVector(40, 0, 0));
			GetWorld()->LineBatcher->DrawLine(p, p + d, FColor::Green, SDPG_MAX, 2, 0.0);
		}
	}

	if (EMPlayerState->bShowVolumeDebug)
	{
		Document->DrawDebugVolumeGraph(GetWorld());
	}

	if (EMPlayerState->bShowSurfaceDebug)
	{
		Document->DrawDebugSurfaceGraphs(GetWorld());
	}

	if (EMPlayerState->bShowMultiplayerDebug)
	{
		Document->DisplayMultiplayerDebugInfo(GetWorld());
	}

	if (EMPlayerState->bShowDesignOptionDebug)
	{
		Document->DisplayDesignOptionDebugInfo(GetWorld());
	}
}

void AEditModelPlayerController::TickInput(float DeltaTime)
{
	// Skip input updates if there are no OS windows with active input focus
	// (unless we're debugging a multiplayer client)
	bool bTickWithoutFocus = false;
#if !UE_BUILD_SHIPPING
	bTickWithoutFocus = IsNetMode(NM_Client);
#endif

	auto viewportClient = Cast<UModumateViewportClient>(GetWorld()->GetGameViewport());
	if (!bTickWithoutFocus && ((viewportClient == nullptr) || !viewportClient->AreWindowsActive()))
	{
		return;
	}

	// Skip input updates if the dimension widget has keyboard focus.
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	UDimensionManager* dimensionManager = gameInstance->DimensionManager;
	ADimensionActor* dimensionActor = dimensionManager->GetActiveActor();
	if (dimensionActor && dimensionActor->DimensionText->Measurement->HasKeyboardFocus())
	{
		return;
	}

	bIsCursorAtAnyWidget = IsCursorOverWidget();
	FSnappedCursor &cursor = EMPlayerState->SnappedCursor;

	if (!CameraInputLock && (CameraController->GetMovementState() == ECameraMovementState::Default))
	{
		GetMousePosition(cursor.ScreenPosition.X, cursor.ScreenPosition.Y);

		// TODO: ideally this wouldn't be necessary if we could prevent preview deltas from polluting snap/cursor data,
		// especially so we could avoid re-applying identical preview deltas multiple frames in a row,
		// but this may require changing our method of raycasting against authoritative collision data.
		Document->ClearPreviewDeltas(GetWorld(), true);

		UpdateMouseHits(DeltaTime);
		UpdateUserSnapPoint();

		if (InteractionHandle != nullptr)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateHandleUpdateUse);

			if (!InteractionHandle->UpdateUse())
			{
				InteractionHandle->EndUse();
				InteractionHandle = nullptr;
			}
		}
		else if (CurrentTool != nullptr)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateToolFrameUpdate);

			CurrentTool->FrameUpdate();
			if (CurrentTool->IsInUse() && CurrentTool->ShowSnapCursorAffordances())
			{
				AddAllOriginAffordances();
			}
		}
	}
	else
	{
		EMPlayerState->SnappedCursor.Visible = false;
	}

	// Do not allow side effect delta generation / application while cleaning objects during tick;
	// this step is meant only to clean objects whose client side representations need updating after underlying data changes.
	Document->CleanObjects(nullptr);

}

FVector AEditModelPlayerController::CalculateViewLocationForSphere(const FSphere &TargetSphere, const FVector &ViewVector, float AspectRatio, float FOV)
{
	//Because of the camera perspective and rotation, clipping of the model was occuring
	// when 0.5f was used. I have changed this to have the model take up '90%' of the thumbnail.
	float captureAlmostHalfFOV = ThumbnailCaptureZoomPercent * FMath::DegreesToRadians(FOV) * 0.5f;
	float captureDistance = AspectRatio * TargetSphere.W / FMath::Tan(captureAlmostHalfFOV);
	return TargetSphere.Center - captureDistance * ViewVector;
}

/*
 * This function draws affordances based on the data in the snapped cursor
 */
void AEditModelPlayerController::AddAllOriginAffordances() const
{
	if (EMPlayerState->SnappedCursor.HasProjectedPosition)
	{
		AddSnapAffordancesToOrigin(EMPlayerState->SnappedCursor.WorldPosition, EMPlayerState->SnappedCursor.ProjectedPosition);
		AddSnapAffordancesToOrigin(EMPlayerState->SnappedCursor.WorldPosition, EMPlayerState->SnappedCursor.AffordanceFrame.Origin);
	}
	else
	{
		switch (EMPlayerState->SnappedCursor.SnapType)
		{
			case ESnapType::CT_CUSTOMSNAPX:
			case ESnapType::CT_CUSTOMSNAPY:
			case ESnapType::CT_CUSTOMSNAPZ:
			case ESnapType::CT_WORLDSNAPX:
			case ESnapType::CT_WORLDSNAPY:
			case ESnapType::CT_WORLDSNAPZ:
				AddSnapAffordancesToOrigin(EMPlayerState->SnappedCursor.AffordanceFrame.Origin, EMPlayerState->SnappedCursor.WorldPosition);
				break;
		};
	}
}

////

/*
* Mouse Functions
*/

void AEditModelPlayerController::SetObjectsSelected(TSet<AModumateObjectInstance*>& Obs, bool bSelected, bool bDeselectOthers)
{
	EMPlayerState->SetObjectsSelected(Obs, bSelected, bDeselectOthers);
}

void AEditModelPlayerController::SetObjectSelected(AModumateObjectInstance *Ob, bool bSelected, bool bDeselectOthers)
{
	TSet< AModumateObjectInstance*> obSet;
	obSet.Add(Ob);
	SetObjectsSelected(obSet, bSelected, bDeselectOthers);
}

void AEditModelPlayerController::HandleDigitKey(int32 DigitKey)
{
	FInputModeUIOnly newMode;
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	auto dimensionManager = gameInstance->DimensionManager;
	if (gameInstance == nullptr || dimensionManager == nullptr)
	{
		return;
	}
	auto dimensionActor = dimensionManager->GetActiveActor();
	if (dimensionActor == nullptr)
	{
		return;
	}

	if (ensure(!dimensionActor->DimensionText->Measurement->HasAnyUserFocus()))
	{
		dimensionActor->DimensionText->Measurement->SetUserFocus(this);

		FText initialText;
		switch (DigitKey)
		{
			// hyphen is also included with the digits so that typing a negative number triggers the focus events
			case 10:
			{
				initialText = FText::FromString(TEXT("-"));
			} break;
			// period is included with the digits so that decimals can by typed without typing 0
			case 11:
			{
				initialText = FText::FromString(TEXT("."));
			} break;
			// 0-9
			default:
			{
				initialText = FText::AsNumber(DigitKey);
			} break;
		}

		dimensionActor->DimensionText->Measurement->SetText(initialText);
	}
}

void AEditModelPlayerController::HandleRawLeftMouseButtonPressed()
{
	ClearUserSnapPoints();

	// If we have a hover handle, then it should have processed the mouse click in its own widget's button press response,
	// immediately during Slate's input routing rather than in player input tick.
	if (!ensure(HoverHandleActor == nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("Hovered handle \"%s\" didn't process button press!"), *HoverHandleActor->GetName());
		return;
	}

	// If we have a handle in-use that didn't process the mouse click because it wasn't hovered, then process it here.
	if (InteractionHandle && ensure(InteractionHandle->IsInUse() && InteractionHandle->Widget))
	{
		ensure(!InteractionHandle->Widget->IsButtonHovered());
		InteractionHandle->EndUse();
		InteractionHandle = nullptr;
		return;
	}

	// TODO: this should no longer be necessary once all reachable leaf widgets handle mouse input.
	// This should only block input from getting handled while we're hovering over benign, non-interactive widgets like borders,
	// as opposed to handles or buttons which should capture the input immediately at the Slate level before
	// the input action has a chance to get processed by the PlayerController.
	if (IsCursorOverWidget())
	{
		return;
	}

	if (CurrentTool)
	{
		if (CurrentTool->IsInUse())
		{
			if (!CurrentTool->EnterNextStage())
			{
				CurrentTool->EndUse();
			}
		}
		else
		{
			CurrentTool->BeginUse();
		}
	}
}

void AEditModelPlayerController::HandleRawLeftMouseButtonReleased()
{
	if (CurrentTool && CurrentTool->IsInUse())
	{
		CurrentTool->HandleMouseUp();
	}
}

void AEditModelPlayerController::SelectAll()
{
	EMPlayerState->SelectAll();
}

void AEditModelPlayerController::SelectInverse()
{
	EMPlayerState->SelectInverse();
}

void AEditModelPlayerController::DeselectAll()
{
	EMPlayerState->DeselectAll();
}

FTransform AEditModelPlayerController::MakeUserSnapPointFromCursor(const FSnappedCursor &cursor) const
{
	FVector location = cursor.WorldPosition;
	FVector normal = cursor.HitNormal;
	FVector tangent = cursor.HitTangent;

	// If the player already has an affordance set (i.e. from a tool or handle),
	// then use its plane, and just project the snap point location to the existing affordance.
	if (EMPlayerState->SnappedCursor.HasAffordanceSet())
	{
		location = EMPlayerState->SnappedCursor.SketchPlaneProject(cursor.WorldPosition);
		normal = EMPlayerState->SnappedCursor.AffordanceFrame.Normal;
		tangent = EMPlayerState->SnappedCursor.AffordanceFrame.Tangent;
	}

	if (normal.IsNearlyZero())
	{
		normal = FVector::UpVector;
	}

	if (tangent.IsNearlyZero())
	{
		FVector biTangent;
		UModumateGeometryStatics::FindBasisVectors(tangent, biTangent, normal);
	}

	FQuat rotation = FQuat::Identity;
	if (!FVector::Parallel(tangent, normal))
	{
		rotation = FRotationMatrix::MakeFromZX(normal, tangent).ToQuat();
	}

	return FTransform(rotation, location);
}

bool AEditModelPlayerController::CanMakeUserSnapPointAtCursor(const FSnappedCursor &cursor) const
{
	FVector potentialSnapPoint = cursor.WorldPosition;
	if (cursor.HasAffordanceSet())
	{
		potentialSnapPoint = cursor.SketchPlaneProject(potentialSnapPoint);
	}

	return (cursor.MouseMode == EMouseMode::Location) &&
		(CurrentTool != nullptr) &&
		CurrentTool->IsActive() &&
		// TODO: remove the requirement that the current tool is not in use;
		// instead, disable their dimension strings while snap point dimension strings are enabled.
		!CurrentTool->IsInUse() &&
		!HasUserSnapPointAtPos(potentialSnapPoint) &&
		((cursor.SnapType == ESnapType::CT_CORNERSNAP) || (cursor.SnapType == ESnapType::CT_MIDSNAP));
}

void AEditModelPlayerController::ToggleUserSnapPoint()
{
	const FSnappedCursor &cursor = EMPlayerState->SnappedCursor;

	if (cursor.Visible)
	{
		int32 removeExistingIndex = INDEX_NONE;

		for (int32 i = 0; i < UserSnapPoints.Num(); ++i)
		{
			const FTransform &userSnapPoint = UserSnapPoints[i];
			FVector snapLocation = userSnapPoint.GetLocation();
			if (snapLocation.Equals(cursor.WorldPosition))
			{
				removeExistingIndex = i;
				break;
			}
		}

		if (removeExistingIndex == INDEX_NONE)
		{
			if (CanMakeUserSnapPointAtCursor(cursor))
			{
				AddUserSnapPoint(MakeUserSnapPointFromCursor(cursor));
			}
		}
		else
		{
			UserSnapPoints.RemoveAt(removeExistingIndex, 0);
		}
	}
}

void AEditModelPlayerController::AddUserSnapPoint(const FTransform &snapPoint)
{
	// TODO: support multiple snap points, if we want to show dimension strings based on which one is snapped to.
	UserSnapPoints.Reset();
	UserSnapPoints.Add(snapPoint);

	EMPlayerState->SnappedCursor.SetAffordanceFrame(
		snapPoint.GetLocation(),
		snapPoint.GetRotation().GetAxisZ(),
		snapPoint.GetRotation().GetAxisX()
	);
}

void AEditModelPlayerController::ClearUserSnapPoints()
{
	UserSnapPoints.Reset();
}

namespace { volatile char* ptr; }
void AEditModelPlayerController::DebugCrash()
{
	UE_LOG(LogTemp, Error, TEXT("DebugCrash"));

	if (bAllowDebugCrash)
	{
		memset((void*)ptr, 0x42, 1024 * 1024 * 20);
	}
}

EAppReturnType::Type AEditModelPlayerController::ShowMessageBox(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	// Skip message boxes if we're playing back input, since we won't be able to dismiss them with a timer on the game thread.
	// TODO: we could record and play back the actual responses to the message boxes, but they so far don't often make an impact on the recorded session content.
	if (InputAutomationComponent && InputAutomationComponent->IsPlaying())
	{
		UE_LOG(LogTemp, Log, TEXT("SKIPPING MESSAGE BOX (type %d) - \"%s\" - \"%s\""), static_cast<int32>(MsgType), Caption, Text);
		return EAppReturnType::Ok;
	}

	// Otherwise forward to the current platform's implementation of message boxes
	return FPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
}

bool AEditModelPlayerController::CanShowFileDialog()
{
	// If we're using input automation to play back input, then don't bother to show the dialog, since it shouldn't affect the current session.
	// TODO: capture the modal dialog action in case we want to know whether it was confirmed or canceled.
	return EMPlayerState && !EMPlayerState->ShowingFileDialog &&
		((InputAutomationComponent == nullptr) || !InputAutomationComponent->IsPlaying());
}

void AEditModelPlayerController::CleanSelectedObjects()
{
	if (!ensure(EMPlayerState && Document))
	{
		return;
	}

	TArray<AModumateObjectInstance *> objectsToClean;
	if (EMPlayerState->SelectedObjects.Num() == 0)
	{
		objectsToClean.Append(Document->GetObjectInstances());
	}
	else
	{
		objectsToClean.Append(EMPlayerState->SelectedObjects.Array());
	}

	for (auto *obj : objectsToClean)
	{
		obj->MarkDirty(EObjectDirtyFlags::All);
	}

	Document->CleanObjects();
}

void AEditModelPlayerController::SetViewGroupObject(const AModumateObjectInstance *ob)
{
	if (ob && (ob->GetObjectType() == EObjectType::OTGroup))
	{
		ModumateCommand(
			FModumateCommand(ModumateCommands::kViewGroupObject)
			.Param(ModumateParameters::kObjectID, ob->ID)
		);
	}
	else
	{
		ModumateCommand(
			FModumateCommand(ModumateCommands::kViewGroupObject)
		);
	}
}

/* User-Document Interface for Blueprints */
void AEditModelPlayerController::UpdateDefaultWallHeight(float newHeight)
{
	if (ensureAlways(Document != nullptr))
	{
		Document->SetDefaultWallHeight(newHeight);
	}
}

float AEditModelPlayerController::GetDefaultWallHeightFromDoc() const
{
	if (ensureAlways(Document != nullptr))
	{
		return Document->GetDefaultWallHeight();
	}
	return 0.f;
}

void AEditModelPlayerController::UpdateDefaultWindowHeightWidth(float newHeight, float newWidth)
{
	Document->SetDefaultWindowHeightWidth(newHeight, newWidth);
}

float AEditModelPlayerController::GetDefaultWindowHeightFromDoc() const
{
	return Document->GetDefaultWindowHeight();
}

float AEditModelPlayerController::GetDefaultRailingsHeightFromDoc() const
{
	return Document->GetDefaultRailHeight();
}

void AEditModelPlayerController::UpdateDefaultJustificationZ(float newValue)
{
	Document->SetDefaultJustificationZ(newValue);
}

void AEditModelPlayerController::UpdateDefaultJustificationXY(float newValue)
{
	Document->SetDefaultJustificationXY(newValue);
}

float AEditModelPlayerController::GetDefaultJustificationZ() const
{
	return Document->GetDefaultJustificationZ();
}

float AEditModelPlayerController::GetDefaultJustificationXY() const
{
	return Document->GetDefaultJustificationXY();
}

void AEditModelPlayerController::ToolAbortUse()
{
	AbortUseTool();
}

void AEditModelPlayerController::IgnoreObjectIDForSnapping(int32 ObjectID)
{
	SnappingIDsToIgnore.Add(ObjectID);
	UpdateMouseTraceParams();
}

void AEditModelPlayerController::ClearIgnoredIDsForSnapping()
{
	SnappingIDsToIgnore.Reset();
	UpdateMouseTraceParams();
}

void AEditModelPlayerController::UpdateMouseTraceParams()
{
	SnappingActorsToIgnore.Reset();
	SnappingActorsToIgnore.Add(this);

	const TArray<AModumateObjectInstance *> objs = Document->GetObjectInstances();
	for (AModumateObjectInstance *obj : objs)
	{
		if (obj && SnappingIDsToIgnore.Contains(obj->ID))
		{
			if (AActor *actorToIgnore = obj->GetActor())
			{
				SnappingActorsToIgnore.Add(actorToIgnore);
			}
		}
	}

	MOITraceObjectQueryParams = FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects);
	MOITraceObjectQueryParams.RemoveObjectTypesToQuery(COLLISION_HANDLE);

	EToolMode curToolMode = CurrentTool ? CurrentTool->GetToolMode() : EToolMode::VE_NONE;
	switch (curToolMode)
	{
	case EToolMode::VE_LINE:
	case EToolMode::VE_RECTANGLE:
	case EToolMode::VE_ROOF_PERIMETER:
	case EToolMode::VE_WALL:
	case EToolMode::VE_FLOOR:
	case EToolMode::VE_CEILING:
	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
	case EToolMode::VE_STAIR:
	case EToolMode::VE_RAIL:
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_STRUCTURELINE:
	case EToolMode::VE_PANEL:
	case EToolMode::VE_MULLION:
	case EToolMode::VE_COUNTERTOP:
		MOITraceObjectQueryParams = FCollisionObjectQueryParams(COLLISION_META_MOI);
		break;
	case EToolMode::VE_SURFACEGRAPH:
		if (CurrentTool->IsInUse())
		{
			MOITraceObjectQueryParams = FCollisionObjectQueryParams(COLLISION_SURFACE_MOI);
		}
		break;
	case EToolMode::VE_PLACEOBJECT:
	case EToolMode::VE_CABINET:
	case EToolMode::VE_FINISH:
	case EToolMode::VE_TRIM:
		MOITraceObjectQueryParams.RemoveObjectTypesToQuery(COLLISION_META_MOI);
		break;
	}

	static const FName MOITraceTag(TEXT("MOITrace"));
	MOITraceQueryParams = FCollisionQueryParams(MOITraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerController), true);
	MOITraceQueryParams.AddIgnoredActors(SnappingActorsToIgnore);
}

bool AEditModelPlayerController::LineTraceSingleAgainstMOIs(struct FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	bool bResultSuccess = GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, MOITraceObjectQueryParams, MOITraceQueryParams);
	
	//If a cutplane is currently culling, check if hit loc is in front of it
	if (bResultSuccess && CurrentCullingCutPlaneID != MOD_ID_NONE)
	{
		const AModumateObjectInstance* cutPlaneMoi = Document->GetObjectById(CurrentCullingCutPlaneID);
		if (cutPlaneMoi && cutPlaneMoi->GetObjectType() == EObjectType::OTCutPlane)
		{
			FVector loc = cutPlaneMoi->GetLocation();
			FVector dir = cutPlaneMoi->GetNormal();
			FPlane cutPlaneCheck = FPlane(loc, dir);
			if (bResultSuccess && cutPlaneCheck.PlaneDot(OutHit.Location) < -PLANAR_DOT_EPSILON)
			{
				// Start a new line trace starting from intersection of cut plane
				FVector intersect = FMath::RayPlaneIntersection(Start, (End - Start).GetSafeNormal(), cutPlaneCheck);
				bResultSuccess = GetWorld()->LineTraceSingleByObjectType(OutHit, intersect, End, MOITraceObjectQueryParams, MOITraceQueryParams);
				
				// Check again if hit is in front of cut plane. ex: Looking from behind a cut plane but hitting a location in front of the cut plane
				if (bResultSuccess && cutPlaneCheck.PlaneDot(OutHit.Location) < -PLANAR_DOT_EPSILON)
				{
					return false;
				}
			}
		}
	}
	return bResultSuccess;
}

bool AEditModelPlayerController::IsCursorOverWidget() const
{
	FSlateApplication &slateApp = FSlateApplication::Get();
	TSharedPtr<FSlateUser> slateUser = slateApp.GetUser(0);
	TSharedPtr<SViewport> gameViewport = slateApp.GetGameViewport();
	FWeakWidgetPath widgetPath = slateUser->GetLastWidgetsUnderCursor();
	TWeakPtr<SWidget> hoverWidgetWeak = widgetPath.IsValid() ? widgetPath.GetLastWidget() : TWeakPtr<SWidget>();
	auto playerHUD = GetEditModelHUD();

	if (!hoverWidgetWeak.IsValid())
	{
		return false;
	}

	TSharedPtr<SWidget> hoverWidget = hoverWidgetWeak.Pin();
	if ((hoverWidget == gameViewport) || hoverWidget->Advanced_IsWindow())
	{
		return false;
	}

	// If the widget that is underneath the cursor is reporting as neither hovered, having mouse capture, nor having focus,
	// then it can be ignored for the purposes of mouse input.
	if (!hoverWidget->IsHovered() && !hoverWidget->HasMouseCapture() && !hoverWidget->HasAnyUserFocus().IsSet())
	{
		return false;
	}

	// If we're hovering over a widget that's used in the world viewport (or one of its children), then don't consider that as a widget that would consume or block the mouse input.
	// TODO: we probably shouldn't rely on Slate widget tags, but for now it's better than making an engine change to add a function like Advanced_IsUwerWidget()
	TSharedPtr<SWidget> potentialWorldWidget = hoverWidget;
	bool bIsWorldWidget = false;
	while (potentialWorldWidget.IsValid() && (potentialWorldWidget != gameViewport))
	{
		if (potentialWorldWidget->GetTag() == playerHUD->WorldViewportWidgetTag)
		{
			return false;
		}

		potentialWorldWidget = potentialWorldWidget->GetParentWidget();
	}

	return true;
}

void AEditModelPlayerController::SetFieldOfViewCommand(float FieldOfView)
{
	ModumateCommand(
		FModumateCommand(ModumateCommands::kSetFOV, true)
		.Param(ModumateParameters::kFieldOfView, FieldOfView));
}

bool AEditModelPlayerController::GetActiveUserSnapPoint(FTransform &outSnapPoint) const
{
	if (EMPlayerState->SnappedCursor.Visible && (UserSnapPoints.Num() > 0) &&
		(EMPlayerState->SnappedCursor.MouseMode == EMouseMode::Location))
	{
		outSnapPoint = UserSnapPoints.Last();
		return true;
	}

	return false;
}

void AEditModelPlayerController::OnToggleFullscreen(bool bIsFullscreen)
{
	static const FString analyticsEventName(TEXT("ToggleFullscreen"));
	UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::View, analyticsEventName);
}

void AEditModelPlayerController::OnHandledInputActionName(FName ActionName, EInputEvent InputEvent)
{
	HandledInputActionEvent.Broadcast(ActionName, InputEvent);

	if (InputEvent == IE_Pressed)
	{
		FString analyticsEventName = FString::Printf(TEXT("Action_%s"), *ActionName.ToString());
		UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Input, analyticsEventName);
	}
}

void AEditModelPlayerController::OnHandledInputAxisName(FName AxisName, float Value)
{
	HandledInputAxisEvent.Broadcast(AxisName, Value);
}

bool AEditModelPlayerController::DistanceBetweenWorldPointsInScreenSpace(const FVector &Point1, const FVector &Point2, float &OutScreenDist) const
{
	OutScreenDist = FLT_MAX;

	FVector2D screenPoint1, screenPoint2;
	if (ProjectWorldLocationToScreen(Point1, screenPoint1) && ProjectWorldLocationToScreen(Point2, screenPoint2))
	{
		OutScreenDist = FVector2D::Distance(screenPoint1, screenPoint2);
		return true;
	}

	return false;
}

bool AEditModelPlayerController::GetScreenScaledDelta(const FVector &Origin, const FVector &Normal, const float DesiredWorldDist, const float MaxScreenDist,
	FVector &OutWorldPos, FVector2D &OutScreenPos) const
{
	if (!ensure(MaxScreenDist >= 0.0f))
	{
		return false;
	}

	FVector endPoint = Origin + DesiredWorldDist * Normal;

	FVector2D origin2D, endPoint2D;
	if (!ProjectWorldLocationToScreen(Origin, origin2D) ||
		!ProjectWorldLocationToScreen(endPoint, endPoint2D))
	{
		return false;
	}

	OutWorldPos = endPoint;
	OutScreenPos = endPoint2D;

	FVector2D endPointDelta2D = endPoint2D - origin2D;
	float endPointScreenDist = endPointDelta2D.Size();
	if (FMath::IsNearlyZero(endPointScreenDist) || (endPointScreenDist < MaxScreenDist))
	{
		return true;
	}

	FVector2D normal2D = endPointDelta2D / endPointScreenDist;
	OutScreenPos = origin2D + MaxScreenDist * normal2D;

	FVector clampedWorldPos, clampedWorldDir;
	if (!DeprojectScreenPositionToWorld(OutScreenPos.X, OutScreenPos.Y, clampedWorldPos, clampedWorldDir))
	{
		return false;
	}

	if (FVector::Parallel(Normal, clampedWorldDir))
	{
		return true;
	}

	FVector tangent = (Normal ^ clampedWorldDir).GetSafeNormal();
	FVector planeNormal = (tangent ^ Normal).GetSafeNormal();
	if (!planeNormal.IsNormalized() || FVector::Orthogonal(clampedWorldDir, planeNormal))
	{
		return false;
	}

	OutWorldPos = FMath::RayPlaneIntersection(clampedWorldPos, clampedWorldDir, FPlane(Origin, planeNormal));
	return true;
}

FMouseWorldHitType AEditModelPlayerController::GetSimulatedStructureHit(const FVector& HitTarget) const
{
	FVector2D hitTargetScreenPos;
	FVector mouseOrigin, mouseDir;
	if (ProjectWorldLocationToScreen(HitTarget, hitTargetScreenPos) &&
		UGameplayStatics::DeprojectScreenToWorld(this, hitTargetScreenPos, mouseOrigin, mouseDir))
	{
		return GetObjectMouseHit(mouseOrigin, mouseDir, true);
	}

	return FMouseWorldHitType();
}

/*
Called every frame, based on mouse mode set in the player state,
we put the mouse's snapped position and projected position into
the SnappedCursor structure, which is read by EditModelPlayerHUD_BP
*/
void AEditModelPlayerController::UpdateMouseHits(float deltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateUpdateMouseHits);

	FVector mouseLoc, mouseDir;
	FMouseWorldHitType baseHit, projectedHit;

	// If we're off-screen (or the window doesn't have focus), skip updating the cursor
	if (!DeprojectMousePositionToWorld(mouseLoc, mouseDir))
	{
		return;
	}

	// Reset frame defaults
	baseHit.Valid = false;
	projectedHit.Valid = false;
	baseHit.Actor = nullptr;
	projectedHit.Actor = nullptr;

	EMPlayerState->SnappedCursor.bValid = false;
	EMPlayerState->SnappedCursor.Visible = false;
	EMPlayerState->SnappedCursor.SnapType = ESnapType::CT_NOSNAP;

	AActor *actorUnderMouse = nullptr;

	// If hovering at widgets, then return with the snapped cursor turned off
	if (bIsCursorAtAnyWidget)
	{
		EMPlayerState->SetHoveredObject(nullptr);
		return;
	}

	EMPlayerState->SnappedCursor.OriginPosition = mouseLoc;
	EMPlayerState->SnappedCursor.OriginDirection = mouseDir;

	// Object mode is used for tools like select and wand, it just returns a simple raycast
	if (EMPlayerState->SnappedCursor.MouseMode == EMouseMode::Object)
	{
		if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("MOUSE MODE: OBJECT")); }

		baseHit = GetObjectMouseHit(mouseLoc, mouseDir, false);

		if (baseHit.Valid)
		{
			const AModumateObjectInstance *hitMOI = Document->ObjectFromActor(baseHit.Actor.Get());
			if (EMPlayerState->bShowDebugSnaps && hitMOI)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("OBJECT HIT #%d, %s"),
					hitMOI->ID, *GetEnumValueString(hitMOI->GetObjectType())
				));
			}
			projectedHit = GetShiftConstrainedMouseHit(baseHit);

			// TODO Is getting attach actor the best way to get Moi from blueprint spawned windows and doors?
			// AActor* attachedActor = baseHit.Actor->GetOwner();

			if (baseHit.Actor->IsA(AModumateObjectInstanceParts::StaticClass()))
			{
				actorUnderMouse = baseHit.Actor->GetOwner();
			}
			else
			{
				actorUnderMouse = baseHit.Actor.Get();
			}
		}
		else
		{
			baseHit = GetSketchPlaneMouseHit(mouseLoc, mouseDir);

			if (baseHit.Valid && EMPlayerState->bShowDebugSnaps)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("SKETCH HIT")));
			}
		}

		if (!baseHit.Valid && EMPlayerState->bShowDebugSnaps)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("NO OBJECT"));
		}
	}
	// Location based mouse hits check against structure and sketch plane, used by floor and wall tools and handles and other tools that want snap
	else if (EMPlayerState->SnappedCursor.MouseMode == EMouseMode::Location)
	{
		EMPlayerState->SnappedCursor.Visible = true;
		if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("MOUSE MODE: LOCATION")); }

		FMouseWorldHitType userPointHit, sketchHit, structuralHit;

		userPointHit = GetUserSnapPointMouseHit(mouseLoc, mouseDir);
		sketchHit = GetSketchPlaneMouseHit(mouseLoc, mouseDir);
		structuralHit = GetObjectMouseHit(mouseLoc, mouseDir, true);

		float userPointDist = FLT_MAX;
		float sketchDist = FLT_MAX;
		float structuralDist = FLT_MAX;

		auto cameraDistance = [this](const FVector &p)
		{
			FVector screenLoc;
			ProjectWorldLocationToScreenWithDistance(p, screenLoc);
			return screenLoc.Z;
		};

		if (userPointHit.Valid)
		{
			userPointDist = cameraDistance(userPointHit.Location);
			if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("USER POINT DIST %f"), userPointDist)); }
		}

		if (sketchHit.Valid)
		{
			sketchDist = cameraDistance(sketchHit.Location);
			if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("SKETCH DIST %f"), sketchDist)); }
		}

		if (structuralHit.Valid)
		{
			structuralDist = cameraDistance(structuralHit.Location);
			if (EMPlayerState->bShowDebugSnaps)
			{
				if (structuralHit.Actor.IsValid())
				{
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("STRUCTURAL ACTOR %s"), *structuralHit.Actor->GetName()));

					const AModumateObjectInstance *hitMOI = Document->ObjectFromActor(structuralHit.Actor.Get());
					if (hitMOI)
					{
						GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("STRUCTURAL MOI %d, %s"),
							hitMOI->ID, *GetEnumValueString(hitMOI->GetObjectType())));
					}
				}

				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("STRUCTURAL DIST %f"), structuralDist));
			}
			actorUnderMouse = structuralHit.Actor.Get();
		}

		// Order of precedence: structural hit first, then user snap points, then a sketch plane hit, then no hit
		// Exception: Prioritize World Axis Snap over Face Snap
		bool bValidSketchWorldSnap = false;
		switch (sketchHit.SnapType)
		{
			case ESnapType::CT_CUSTOMSNAPX:
			case ESnapType::CT_CUSTOMSNAPY:
			case ESnapType::CT_CUSTOMSNAPZ:
			case ESnapType::CT_WORLDSNAPX:
			case ESnapType::CT_WORLDSNAPY:
			case ESnapType::CT_WORLDSNAPZ:
			case ESnapType::CT_WORLDSNAPXY:
				bValidSketchWorldSnap = true;
				break;
		};

		bool bCombineStructuralSketchSnaps = false;
		if (bValidSketchWorldSnap && structuralHit.Valid)
		{
			switch (structuralHit.SnapType)
			{
			case ESnapType::CT_FACESELECT:
			case ESnapType::CT_EDGESNAP:
				bCombineStructuralSketchSnaps = true;
				break;
			}
		}

		if (structuralHit.Valid && !bCombineStructuralSketchSnaps)
		{
			if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("STRUCTURAL")); }
			projectedHit = GetShiftConstrainedMouseHit(structuralHit);
			baseHit = structuralHit;
		}
		else if (userPointHit.Valid)
		{
			if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("USER POINT")); }
			projectedHit = GetShiftConstrainedMouseHit(userPointHit);
			baseHit = userPointHit;
		}
		else if (sketchDist < (structuralDist - StructuralSnapPreferenceEpsilon) || bCombineStructuralSketchSnaps)
		{
			// For combined structural/sketch snaps on edges, we want to override the position to be where the edge and sketch axis intersect if possible.
			FVector lineIntersection;
			float sketchRayDist, structuralEdgeDist;
			if (bCombineStructuralSketchSnaps && (structuralHit.SnapType == ESnapType::CT_EDGESNAP) && !FVector::Parallel(sketchHit.Normal, structuralHit.EdgeDir) &&
				UModumateGeometryStatics::RayIntersection3D(sketchHit.Origin, sketchHit.Normal, structuralHit.Origin, structuralHit.EdgeDir, lineIntersection, sketchRayDist, structuralEdgeDist, false))
			{
				sketchHit.Location = lineIntersection;
			}

			// If this sketch hit is being used because of a world or custom axis snap, then allow it to just be a snapped structural hit.
			// That means we need to redo the structural mouse hit with the snapped screen position of the sketch hit, and report the new location.
			// This allows physical hits against objects that are aligned with snap axes to still report data like hit normals and actors.
			FVector2D sketchHitScreenPos;
			FVector sketchMouseOrigin, sketchMouseDir;
			if (bCombineStructuralSketchSnaps && sketchHit.Valid && structuralHit.Valid &&
				ProjectWorldLocationToScreen(sketchHit.Location, sketchHitScreenPos) &&
				UGameplayStatics::DeprojectScreenToWorld(this, sketchHitScreenPos, sketchMouseOrigin, sketchMouseDir))
			{
				FMouseWorldHitType snappedStructuralHit = GetObjectMouseHit(sketchMouseOrigin, sketchMouseDir, true);

				float screenSpaceDist;
				if (snappedStructuralHit.Valid &&
					DistanceBetweenWorldPointsInScreenSpace(snappedStructuralHit.Location, sketchHit.Location, screenSpaceDist) &&
					(screenSpaceDist < SnapPointMaxScreenDistance))
				{
					// Only inherit the normal and actor from the structural hit, because for dimensioning purposes, the world-snapped sketch hit location is more precise.
					// TODO: resolve multiple constraints of being snapped/projected along axes, and also being planar with the other definitional points of the hit object.
					sketchHit.Normal = snappedStructuralHit.Normal;
					sketchHit.Actor = snappedStructuralHit.Actor;
				}
			}

			if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("SKETCH")); }
			projectedHit = GetShiftConstrainedMouseHit(sketchHit);
			baseHit = sketchHit;
		}
		else
		{
			if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("NO HIT")); }
			baseHit.Valid = false;
		}
	}

	EMPlayerState->SnappedCursor.HitTangent = FVector::ZeroVector;
	EMPlayerState->SnappedCursor.HitNormal = FVector::UpVector;
	EMPlayerState->SnappedCursor.CP1 = -1;
	EMPlayerState->SnappedCursor.CP2 = -1;

	if (baseHit.Valid)
	{
		EMPlayerState->SnappedCursor.HasProjectedPosition = false;
		FVector projectedLocation = baseHit.Location;

		if (projectedHit.Valid)
		{
			if (EMPlayerState->bShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("HAVE PROJECTED")); }
			EMPlayerState->SnappedCursor.HasProjectedPosition = true;

			// Fully inherit the projected hit, which for now clears any structural information from the base hit.
			ESnapType baseSnapType = baseHit.SnapType;
			baseHit = projectedHit;
			baseHit.SnapType = baseSnapType;

			// If we have a projected hit, then similar to snapped sketch hits, we want to perform a structural hit in order to have accurate
			// non-location hit result data for the projected location, so that tools can filter results accordingly.
			FVector2D projectedHitScreenPos;
			FVector projectedHitMouseOrigin, projectedHitMouseDir;
			if (ProjectWorldLocationToScreen(projectedHit.Location, projectedHitScreenPos) &&
				UGameplayStatics::DeprojectScreenToWorld(this, projectedHitScreenPos, projectedHitMouseOrigin, projectedHitMouseDir))
			{
				FMouseWorldHitType projectedStructuralHit = GetObjectMouseHit(projectedHitMouseOrigin, projectedHitMouseDir, true);

				float screenSpaceDist;
				if (projectedStructuralHit.Valid &&
					DistanceBetweenWorldPointsInScreenSpace(projectedStructuralHit.Location, projectedHit.Location, screenSpaceDist) &&
					(screenSpaceDist < SnapPointMaxScreenDistance))
				{
					baseHit = projectedStructuralHit;
					baseHit.SnapType = baseSnapType;
					baseHit.Location = projectedHit.Location;
				}
			}
		}

		EMPlayerState->SnappedCursor.bValid = true;
		EMPlayerState->SnappedCursor.SnapType = baseHit.SnapType;
		EMPlayerState->SnappedCursor.WorldPosition = baseHit.Location;
		ProjectWorldLocationToScreen(baseHit.Location, EMPlayerState->SnappedCursor.ScreenPosition);
		EMPlayerState->SnappedCursor.ProjectedPosition = projectedLocation;
		EMPlayerState->SnappedCursor.Actor = baseHit.Actor.Get();
		EMPlayerState->SnappedCursor.CP1 = baseHit.CP1;
		EMPlayerState->SnappedCursor.CP2 = baseHit.CP2;
		EMPlayerState->SnappedCursor.HitNormal = baseHit.Normal;
		EMPlayerState->SnappedCursor.HitTangent = baseHit.EdgeDir;
	}

	if (EMPlayerState->bShowDebugSnaps)
	{
		FString msg;
		switch (EMPlayerState->SnappedCursor.SnapType)
		{
			case ESnapType::CT_NOSNAP:msg = TEXT("None"); break;
			case ESnapType::CT_CORNERSNAP:msg = TEXT("Corner"); break;
			case ESnapType::CT_MIDSNAP:msg = TEXT("Midpoint"); break;
			case ESnapType::CT_EDGESNAP:msg = TEXT("Edge"); break;
			case ESnapType::CT_ALIGNMENTSNAP:msg = TEXT("Alignment"); break;
			case ESnapType::CT_FACESELECT:msg = TEXT("Face"); break;
			case ESnapType::CT_WORLDSNAPX:msg = TEXT("World X"); break;
			case ESnapType::CT_WORLDSNAPY:msg = TEXT("World Y"); break;
			case ESnapType::CT_WORLDSNAPXY:msg = TEXT("Origin"); break;
			case ESnapType::CT_WORLDSNAPZ:msg = TEXT("World Z"); break;
			case ESnapType::CT_USERPOINTSNAP:msg = TEXT("User Point"); break;
			case ESnapType::CT_CUSTOMSNAPX:msg = TEXT("CustomX"); break;
			case ESnapType::CT_CUSTOMSNAPY:msg = TEXT("CustomY"); break;
			case ESnapType::CT_CUSTOMSNAPZ:msg = TEXT("CustomZ"); break;
			default: msg = TEXT("Unknown"); break;
		};
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, *FString::Printf(TEXT("Snap Type: %s"), *msg));
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, *FString::Printf(TEXT("CP: %d %d"), EMPlayerState->SnappedCursor.CP1, EMPlayerState->SnappedCursor.CP2));
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, *FString::Printf(TEXT("Cursor Actor: %s"),
			EMPlayerState->SnappedCursor.Actor ? *EMPlayerState->SnappedCursor.Actor->GetName() : TEXT("None")));
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, *FString::Printf(TEXT("Normal: %s"), *EMPlayerState->SnappedCursor.HitNormal.ToString()));
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, *FString::Printf(TEXT("Location: %s"), *EMPlayerState->SnappedCursor.WorldPosition.ToString()));
	}

	AModumateObjectInstance *newHoveredObject = nullptr;

	if (InteractionHandle == nullptr && actorUnderMouse)
	{
		newHoveredObject = Document->ObjectFromActor(actorUnderMouse);
	}

	// Make sure hovered object is always in a valid group view
	newHoveredObject = EMPlayerState->GetValidHoveredObjectInView(newHoveredObject);
	EMPlayerState->SetHoveredObject(newHoveredObject);

	// Automatic user snap creation is disabled
#if 0
	if (LastSnappedCursor == EMPlayerState->SnappedCursor)
	{
		if (CanMakeUserSnapPointAtCursor(EMPlayerState->SnappedCursor))
		{
			// If we just passed the delay time, then start making a user snap point.
			float newSnapCreationElapsed = UserSnapAutoCreationElapsed + deltaTime;
			if ((newSnapCreationElapsed >= UserSnapAutoCreationDelay) &&
				(UserSnapAutoCreationElapsed < UserSnapAutoCreationDelay))
			{
				UserSnapAutoCreationElapsed = UserSnapAutoCreationDelay;
				PendingUserSnapPoint = MakeUserSnapPointFromCursor(EMPlayerState->SnappedCursor);
				OnStartCreateUserSnapPoint.Broadcast();
			}
			else
			{
				UserSnapAutoCreationElapsed = newSnapCreationElapsed;
			}

			if (UserSnapAutoCreationElapsed >= UserSnapAutoCreationDelay)
			{
				float postDelayElapsedTime = (UserSnapAutoCreationElapsed - UserSnapAutoCreationDelay);
				if (postDelayElapsedTime > UserSnapAutoCreationDuration)
				{
					AddUserSnapPoint(MakeUserSnapPointFromCursor(EMPlayerState->SnappedCursor));
					OnFinishCreateUserSnapPoint.Broadcast();
					UserSnapAutoCreationElapsed = 0.0f;
				}
			}
		}
	}
	else
	{
		if (UserSnapAutoCreationElapsed > UserSnapAutoCreationDelay)
		{
			OnCancelCreateUserSnapPoint.Broadcast();
		}

		UserSnapAutoCreationElapsed = 0.0f;
		LastSnappedCursor = EMPlayerState->SnappedCursor;
	}
#endif
}

FModumateFunctionParameterSet AEditModelPlayerController::ModumateCommand(const FModumateCommand &cmd)
{
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	return gameInstance->DoModumateCommand(cmd);
}

bool AEditModelPlayerController::SnapDistAlongAffordance(FVector& SnappedPosition, const FVector& AffordanceOrigin, const FVector& AffordanceDir) const
{
	// About how many pixels should the cursor snap between, based on the current camera distance and preferred snap units?
	static constexpr double screenSpaceIncrement = 8.0;

	// What is the minimum camera distance to target point that will affect screen-scaled snapping?
	static constexpr float minCamDist = 2.0;

	// Determine the lowest we're willing to snap, in world units, based on user preferences
	double worldMinIncrement = 0.0;
	TArray<int32> incrementMultipliers;
	EDimensionUnits snapDimensionType = EDimensionUnits::DU_Imperial;
	UModumateDocument* document = GetDocument();
	if (document)
	{
		auto& docSettings = document->GetCurrentSettings();
		snapDimensionType = docSettings.DimensionType;
		worldMinIncrement = docSettings.MinimumDistanceIncrement;
	}

	switch (snapDimensionType)
	{
	case EDimensionUnits::DU_Imperial:
		incrementMultipliers = FDocumentSettings::ImperialDistIncrementMultipliers;
		if (worldMinIncrement == 0.0)
		{
			worldMinIncrement = FDocumentSettings::MinImperialDistIncrementCM;
		}
		break;
	case EDimensionUnits::DU_Metric:
		incrementMultipliers = FDocumentSettings::MetricDistIncrementMultipliers;
		if (worldMinIncrement == 0.0)
		{
			worldMinIncrement = FDocumentSettings::MinMetricDistIncrementCM;
		}
		break;
	default:
		return false;
	}

	// Based on camera distance and FOV, find the screen-space-to-world-space factor so we can find the world snap increment.
	int32 viewportX, viewportY;
	GetViewportSize(viewportX, viewportY);
	APlayerCameraManager* camManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	FVector camPos = camManager->GetCameraLocation();
	float distFromCam = FMath::Max(FVector::Dist(SnappedPosition, camPos), minCamDist);
	double screenToWorldFactor = 2.0 * distFromCam * FMath::Tan(FMath::DegreesToRadians(0.5 * camManager->GetFOVAngle())) / viewportX;
	double worldSpaceIncrement = screenToWorldFactor * screenSpaceIncrement;

	// Find the largest increment that is smaller than the world-space scaled version of the screen space snap increment
	int32 multiplierIdx = 0;
	int32 numMultipliers = incrementMultipliers.Num();
	int32 curMultiplier = incrementMultipliers[multiplierIdx];
	int32 lastMultiplier = curMultiplier;
	while ((curMultiplier * worldMinIncrement) < worldSpaceIncrement)
	{
		lastMultiplier = curMultiplier;

		if (multiplierIdx < (numMultipliers - 1))
		{
			curMultiplier = incrementMultipliers[++multiplierIdx];
		}
		else
		{
			curMultiplier *= 2;
		}
	}
	worldSpaceIncrement = worldMinIncrement * lastMultiplier;

	// Using the appropriately screen-scaled world-space snap increment, snap the position along the affordance ray.
	FVector deltaFromOrigin = SnappedPosition - AffordanceOrigin;
	float distanceAlongAffordance = 0.0f;
	FVector snapDir = AffordanceDir;

	// If a normalized affordance direction is specified, then snap along that ray;
	// otherwise, snap along whatever ray the mouse is pointing relative to the origin.
	if (AffordanceDir.IsNormalized())
	{
		distanceAlongAffordance = deltaFromOrigin | AffordanceDir;
	}
	else
	{
		distanceAlongAffordance = deltaFromOrigin.Size();
		snapDir = FMath::IsNearlyZero(distanceAlongAffordance) ? FVector::ZeroVector : (deltaFromOrigin / distanceAlongAffordance);
	}

	distanceAlongAffordance = worldSpaceIncrement * FMath::RoundHalfFromZero(distanceAlongAffordance / worldSpaceIncrement);
	SnappedPosition = AffordanceOrigin + (distanceAlongAffordance * snapDir);

	return true;
}

bool AEditModelPlayerController::ValidateVirtualHit(const FVector &MouseOrigin, const FVector &MouseDir,
	const FVector &HitPoint, float CurObjectHitDist, float CurVirtualHitDist, float MaxScreenDist, float &OutRayDist) const
{
	// The virtual hit point needs to be within a maximum screen distance, and ahead of the mouse
	OutRayDist = (HitPoint - MouseOrigin) | MouseDir;
	float screenSpaceDist;
	if (DistanceBetweenWorldPointsInScreenSpace(HitPoint, MouseOrigin, screenSpaceDist) &&
		(screenSpaceDist < MaxScreenDist) && (OutRayDist > KINDA_SMALL_NUMBER) && (OutRayDist < CurVirtualHitDist))
	{
		// If virtual hit point is a candidate, make sure it's either
		// in front of our current object hit distance, or verify that it's not occluded.
		if ((CurObjectHitDist < FLT_MAX) && (OutRayDist < CurObjectHitDist))
		{
			return true;
		}
		else
		{
			FHitResult occlusionResult;
			FVector occlusionTraceEnd = HitPoint - (VirtualHitBias * MouseDir);
			return !LineTraceSingleAgainstMOIs(occlusionResult, MouseOrigin, occlusionTraceEnd);
		}
	}

	return false;
}

bool AEditModelPlayerController::FindBestMousePointHit(const TArray<FStructurePoint> &Points,
	const FVector &MouseOrigin, const FVector &MouseDir, float CurObjectHitDist,
	int32 &OutBestIndex, float &OutBestRayDist) const
{
	OutBestIndex = INDEX_NONE;
	OutBestRayDist = FLT_MAX;

	int32 numPoints = Points.Num();
	for (int32 pointIdx = 0; pointIdx < numPoints; ++pointIdx)
	{
		const FStructurePoint& structurePoint = Points[pointIdx];

		// See if this is our best valid point hit
		float rayDist;
		if (ValidateVirtualHit(MouseOrigin, MouseDir, structurePoint.Point,
			CurObjectHitDist, OutBestRayDist, SnapPointMaxScreenDistance, rayDist))
		{
			auto nextPoint = Points[pointIdx];

			//If we have a previous best, run a quick heuristic on it to
			// determine if we should replace it
			if (OutBestIndex != INDEX_NONE)
			{
				//If the 'next' point returned from ValidateVirtualHit
				// is a midpoint, we need to score it worse so that we
				// prioritize corners over mids.
				if (nextPoint.PointType == EPointType::Middle)
				{
					
					rayDist = rayDist * MidPointHitBias;
				}

				if (rayDist < OutBestRayDist)
				{
					OutBestRayDist = rayDist;
					OutBestIndex = pointIdx;
				}
			}

			//Otherwise, just flat out replace it...
			else
			{
				OutBestRayDist = nextPoint.PointType == EPointType::Middle ? (rayDist * MidPointHitBias) : rayDist;
				OutBestIndex = pointIdx;
			}

		}
	}

	return (OutBestIndex != INDEX_NONE);
}

bool AEditModelPlayerController::FindBestMouseLineHit(const TArray<TPair<FVector, FVector>> &Lines,
	const FVector &MouseOrigin, const FVector &MouseDir, float CurObjectHitDist,
	int32 &OutBestIndex, FVector &OutBestIntersection, float &OutBestRayDist) const
{
	OutBestIndex = INDEX_NONE;
	OutBestIntersection = MouseOrigin;
	OutBestRayDist = FLT_MAX;

	int32 numLines = Lines.Num();
	for (int32 lineIdx = 0; lineIdx < numLines; ++lineIdx)
	{
		const auto &line = Lines[lineIdx];
		const FVector &lineStart = line.Key;
		const FVector &lineEnd = line.Value;
		FVector lineDelta = lineEnd - lineStart;
		float lineLength = lineDelta.Size();
		if (FMath::IsNearlyZero(lineLength, KINDA_SMALL_NUMBER))
		{
			continue;
		}
		FVector lineDir = lineDelta / lineLength;

		FVector lineIntercept, rayIntercept;
		float lineRayDist;
		if (!UModumateGeometryStatics::FindShortestDistanceBetweenRays(lineStart, lineDir, MouseOrigin, MouseDir, lineIntercept, rayIntercept, lineRayDist))
		{
			continue;
		}

		// Make sure the intercept point is on the line segment, otherwise we're just "close" to the infinite line defined by the segment
		float intersectLengthOnLine = (lineIntercept - lineStart) | lineDir;
		if (!FMath::IsWithin(intersectLengthOnLine, -KINDA_SMALL_NUMBER, lineLength + KINDA_SMALL_NUMBER))
		{
			continue;
		}

		// See if this is our best valid line hit
		float rayDist;
		if (ValidateVirtualHit(MouseOrigin, MouseDir, lineIntercept,
			CurObjectHitDist, OutBestRayDist, SnapLineMaxScreenDistance, rayDist))
		{
			OutBestRayDist = rayDist;
			OutBestIndex = lineIdx;
			OutBestIntersection = lineIntercept;
		}
	}

	return (OutBestIndex != INDEX_NONE);
}

FMouseWorldHitType AEditModelPlayerController::GetAffordanceHit(const FVector &mouseLoc, const FVector &mouseDir, const FAffordanceFrame &affordance, bool allowZSnap) const
{
	FMouseWorldHitType ret;
	ret.Valid = false;
	ret.Actor = nullptr;
	ret.SnapType = ESnapType::CT_NOSNAP;

	int32 affordanceDimensions = allowZSnap ? 3 : 2;

	// If we're near the affordance origin, snap to it
	float screenSpaceDist;
	if (DistanceBetweenWorldPointsInScreenSpace(mouseLoc, affordance.Origin, screenSpaceDist) && (screenSpaceDist < SnapPointMaxScreenDistance))
	{
		ret.Valid = true;
		ret.SnapType = ESnapType::CT_WORLDSNAPXY;
		ret.Location = affordance.Origin;
	}
	// Otherwise, if we have a custom affordance direction (purple axes), prefer those .. affordance.Tangent will be ZeroVector otherwise
	else if (affordance.Normal.IsNormalized() && affordance.Tangent.IsNormalized())
	{
		// Assumption: the affordance direction is along the sketch plane and so is perpendicular to the sketch plane normal
		// TODO: when arbitrary affordances are added for ambiguous snaps (edges, corners), resolve whether each affordance has its own sketch plane
		// or if we must resolve gimbal lock issues for cases where the main affordance is parallel to the sketch plane normal

		FVector customBasis[] = { affordance.Tangent,FVector::CrossProduct(affordance.Normal,affordance.Tangent).GetSafeNormal(),affordance.Normal };
		ESnapType customSnaps[] = { ESnapType::CT_WORLDSNAPX,ESnapType::CT_WORLDSNAPY,ESnapType::CT_WORLDSNAPZ };

		for (int32 i = 0; i < affordanceDimensions; ++i)
		{
			const FVector& affordanceDir = customBasis[i];
			FVector affordanceIntercept, mouseIntercept;
			float distance = 0.0f;
			if (UModumateGeometryStatics::FindShortestDistanceBetweenRays(affordance.Origin, affordanceDir, mouseLoc, mouseDir, affordanceIntercept, mouseIntercept, distance))
			{
				if (DistanceBetweenWorldPointsInScreenSpace(affordanceIntercept, mouseIntercept, screenSpaceDist) && (screenSpaceDist < SnapLineMaxScreenDistance))
				{
					// Quantize the snap location along the affordance direction
					SnapDistAlongAffordance(affordanceIntercept, affordance.Origin, affordanceDir);

					// If this is the first hit or closer than the last one...
					if (!ret.Valid || (affordanceIntercept - mouseLoc).Size() < (ret.Location - mouseLoc).Size())
					{
						ret.SnapType = customSnaps[i];
						ret.Origin = affordance.Origin;
						ret.Location = affordanceIntercept;
						ret.Normal = customBasis[i];
						ret.Valid = true;
					}
				}
			}
		}
	}
	return ret;
}

/*
Support function returns a hit point on the sketch plane with an auto snap either to the snapping origin or the custom affordance origin
*/
FMouseWorldHitType AEditModelPlayerController::GetSketchPlaneMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const
{
	// Tool modes and handles determine for themselves if they want z-axis affordances (off sketch plane)

	/*
	Sketch plane hit rules:
	First, see if we want to snap to any of the basis vectors originating at the affordance. 
	If we receive no basis vector snap, then project the mouse ray onto the sketch plane to determine the hit
	Assumption: the affordance origin is on the sketch plane
	*/
	FMouseWorldHitType ret;

	if (EMPlayerState->SnappedCursor.HasAffordanceSet())
	{
		ret = GetAffordanceHit(mouseLoc, mouseDir, EMPlayerState->SnappedCursor.AffordanceFrame, EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap);
	}

	// Sketch plane hits come back with world x, y or z indicated .. convert to custom
	if (ret.Valid)
	{
		switch (ret.SnapType)
		{
			case ESnapType::CT_WORLDSNAPX: ret.SnapType = ESnapType::CT_CUSTOMSNAPX; break;
			case ESnapType::CT_WORLDSNAPY: ret.SnapType = ESnapType::CT_CUSTOMSNAPY; break;
			case ESnapType::CT_WORLDSNAPZ: ret.SnapType = ESnapType::CT_CUSTOMSNAPZ; break;
		}
	}
	// We got no custom hit, so go for a global direction hit
	else if (EMPlayerState->SnappedCursor.bSnapGlobalAxes)
	{
		FAffordanceFrame globalAffordance;
		globalAffordance.Origin = EMPlayerState->SnappedCursor.AffordanceFrame.Origin;
		globalAffordance.Normal = FVector::UpVector;
		globalAffordance.Tangent = FVector::LeftVector;

		ret = GetAffordanceHit(mouseLoc, mouseDir, globalAffordance, EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap);
	}

	// If we didn't snap to any affordance lines, just take the unsnapped plane hit
	if (!ret.Valid)
	{
		ret.Valid = EMPlayerState->SnappedCursor.TryGetRaySketchPlaneIntersection(mouseLoc, mouseDir, ret.Location);
		ret.Normal = EMPlayerState->SnappedCursor.AffordanceFrame.Normal;

		if (ret.Valid && EMPlayerState->SnappedCursor.HasAffordanceSet())
		{
			SnapDistAlongAffordance(ret.Location, EMPlayerState->SnappedCursor.AffordanceFrame.Origin, FVector::ZeroVector);
		}
		//if ret.Valid is false ray did not intersect sketch plane
		if(!ret.Valid)
		{
			//if ray does not intersect sketch plane clamp the ray at 10 meters from camera.
			ret.Location = (mouseLoc + (mouseDir * MaxRayLengthOnHitMiss));
			ret.Valid = true;
		}
	}

	if (ret.Valid)
	{
		ProjectWorldLocationToScreen(ret.Location, EMPlayerState->SnappedCursor.ScreenPosition);
	}

	return ret;
}

FMouseWorldHitType AEditModelPlayerController::GetUserSnapPointMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const
{
	FMouseWorldHitType ret;
	ret.Valid = false;
	ret.Actor = nullptr;
	ret.SnapType = ESnapType::CT_NOSNAP;

	float bestScreenDist = 0;
	for (auto &userSnapPoint : UserSnapPoints)
	{
		FAffordanceFrame affordance;
		affordance.Origin = userSnapPoint.GetLocation();
		affordance.Normal = userSnapPoint.GetRotation().GetUpVector();
		affordance.Tangent = userSnapPoint.GetRotation().GetRightVector();
		// TODO: do user snaps care about the z axis? 'false' here means they don't
		FMouseWorldHitType trySnap = GetAffordanceHit(mouseLoc,mouseDir,affordance,false);
		if (trySnap.Valid)
		{
			trySnap.SnapType = ESnapType::CT_USERPOINTSNAP;
			float screenSpaceDist = FLT_MAX;
			if (!ret.Valid || (DistanceBetweenWorldPointsInScreenSpace(mouseLoc, trySnap.Location, screenSpaceDist) && (screenSpaceDist < bestScreenDist)))
			{
				bestScreenDist = screenSpaceDist;
				ret = trySnap;
				ret.EdgeDir = userSnapPoint.GetRotation().GetAxisX();
				ret.Normal = userSnapPoint.GetRotation().GetAxisZ();
			}
		}
	}
	return ret;
}

/*
Support function returns an object face hit, using the engine's raycast
*/
FMouseWorldHitType AEditModelPlayerController::GetObjectMouseHit(const FVector& mouseLoc, const FVector& mouseDir, bool bCheckSnapping) const
{
	// Find the MOI (if any) that we hit
	FMouseWorldHitType objectHit;
	float objectHitDist = FLT_MAX;
	FHitResult hitSingleResult;
	AActor* directHitActor = nullptr;
	const AModumateObjectInstance* directHitMOI = nullptr;
	FVector directHitNormal(ForceInitToZero);
	TSet<int32> directHitPolyNeighbors;
	bool bDirectHitPolygon = false;
	const FSnappedCursor& cursor = EMPlayerState->SnappedCursor;

	if (LineTraceSingleAgainstMOIs(hitSingleResult, mouseLoc, mouseLoc + MaxRaycastDist * mouseDir))
	{
		objectHit.Valid = true;
		objectHit.Actor = hitSingleResult.Actor.Get();
		objectHit.Location = hitSingleResult.Location;
		objectHit.Normal = hitSingleResult.Normal;

		directHitMOI = Document->ObjectFromActor(objectHit.Actor.Get());
		EObjectType objectType = directHitMOI ? directHitMOI->GetObjectType() : EObjectType::OTNone;
		// TODO: this should be an interface function or there should be a way to find the planar object types
		if ((objectType == EObjectType::OTMetaPlane) || (objectType == EObjectType::OTSurfacePolygon))
		{
			FVector moiNormal = directHitMOI->GetNormal();
			FPlane plane = FPlane(directHitMOI->GetLocation(), moiNormal);
			objectHit.Normal = (moiNormal | objectHit.Normal) > 0 ? moiNormal : -moiNormal;
			objectHit.Location = FVector::PointPlaneProject(objectHit.Location, plane);

			// If we've directly hit a polygon, then we want to exclude objects that are coplanar and disconnected.
			// Note: this is only necessary and useful for SurfacePolygons, because multiple SurfaceEdges are allowed to be colinear and coexist,
			// and peninsula/contained edges are considered connected only for SurfacePolygons rather than MetaPlanes.
			if (objectType == EObjectType::OTSurfacePolygon)
			{
				TArray<int32> connectedIDs;
				directHitMOI->GetConnectedIDs(connectedIDs);
				directHitPolyNeighbors.Append(connectedIDs);
				bDirectHitPolygon = true;
			}
		}

		objectHit.SnapType = ESnapType::CT_FACESELECT;
		objectHitDist = FVector::Dist(objectHit.Location, mouseLoc);

		// Save off direct hit data so we can use it to populate other virtual hit results
		directHitActor = objectHit.Actor.Get();
		directHitNormal = objectHit.Normal;
	}

	int32 bestVirtualHitIndex;
	float bestVirtualHitDist;
	FVector bestLineIntersection;
	FPlane objectHitPlane(objectHit.Location, objectHit.Normal);
	int32 mouseQueryBitfield = MOITraceObjectQueryParams.GetQueryBitfield();

	// Validate all points/lines if we've directly hit a polygon.
	auto validateStructurePoint = [bDirectHitPolygon, objectHitPlane, &directHitPolyNeighbors](const FStructurePoint& StructurePoint) -> bool
	{
		return !bDirectHitPolygon || directHitPolyNeighbors.Contains(StructurePoint.ObjID) ||
			!FMath::IsNearlyZero(objectHitPlane.PlaneDot(StructurePoint.Point), PLANAR_DOT_EPSILON);
	};
	auto validateStructureLine = [bDirectHitPolygon, objectHitPlane, &directHitPolyNeighbors](const FStructureLine& StructureLine) -> bool
	{
		return !bDirectHitPolygon || directHitPolyNeighbors.Contains(StructureLine.ObjID) ||
			!FMath::IsNearlyZero(objectHitPlane.PlaneDot(StructureLine.P1), PLANAR_DOT_EPSILON) ||
			!FMath::IsNearlyZero(objectHitPlane.PlaneDot(StructureLine.P2), PLANAR_DOT_EPSILON);
	};
	static TArray<FStructurePoint> tempStructurePoints;
	static TArray<FStructureLine> tempStructureLines;
	tempStructurePoints.Reset();
	tempStructureLines.Reset();

	// After tracing for a direct hit result against a MOI, now check if it would be overridden by a snap point or line if desired
	if (bCheckSnapping)
	{
		// Update the snapping view (lines and points that can be snapped to)
		SnappingView->UpdateSnapPoints(SnappingIDsToIgnore, mouseQueryBitfield, true, false);

		// First, filter the snapping view based on eligible objects, now that we can filter them based on their snapping data
		Algo::CopyIf(SnappingView->Corners, tempStructurePoints, validateStructurePoint);
		Algo::CopyIf(SnappingView->LineSegments, tempStructureLines, validateStructureLine);

		// Then, transform them to raw positions for the purposes of hit selection
		CurHitPointLocations.Reset();
		CurHitLineLocations.Reset();
		Algo::Transform(tempStructurePoints, CurHitPointLocations, [](const FStructurePoint &point) { return point; });
		Algo::Transform(tempStructureLines, CurHitLineLocations,[](const FStructureLine &line) { return TPair<FVector, FVector>(line.P1, line.P2); });

		if (FindBestMousePointHit(CurHitPointLocations, mouseLoc, mouseDir, objectHitDist, bestVirtualHitIndex, bestVirtualHitDist))
		{
			FStructurePoint &bestPoint = tempStructurePoints[bestVirtualHitIndex];

			objectHit.Valid = true;
			objectHit.Location = bestPoint.Point;
			objectHit.EdgeDir = bestPoint.Direction;
			objectHit.CP1 = bestPoint.CP1;
			objectHit.CP2 = bestPoint.CP2;

			AModumateObjectInstance *bestObj = Document->GetObjectById(bestPoint.ObjID);
			if (bestObj)
			{
				objectHit.Actor = bestObj->GetActor();
				objectHit.SnapType = (bestPoint.PointType == EPointType::Corner) ? ESnapType::CT_CORNERSNAP : ESnapType::CT_MIDSNAP;
				if (objectHit.Actor == directHitActor)
				{
					objectHit.Normal = directHitNormal;
				}
			}
			else if (bestPoint.ObjID == MOD_ID_NONE)
			{
				objectHit.SnapType = ESnapType::CT_CORNERSNAP;
			}
			else
			{
				objectHit.Valid = false;
			}
		}
		else if (FindBestMouseLineHit(CurHitLineLocations, mouseLoc, mouseDir, objectHitDist, bestVirtualHitIndex, bestLineIntersection, bestVirtualHitDist))
		{
			FStructureLine &bestLine = tempStructureLines[bestVirtualHitIndex];

			objectHit.Valid = true;
			objectHit.Location = bestLineIntersection;
			objectHit.Origin = bestLine.P1;
			objectHit.EdgeDir = (bestLine.P2 - bestLine.P1).GetSafeNormal();
			objectHit.CP1 = bestLine.CP1;
			objectHit.CP2 = bestLine.CP2;

			AModumateObjectInstance *bestObj = Document->GetObjectById(bestLine.ObjID);
			if (bestObj)
			{
				objectHit.SnapType = ESnapType::CT_EDGESNAP;
				objectHit.Actor = bestObj->GetActor();
				if (objectHit.Actor == directHitActor)
				{
					objectHit.Normal = directHitNormal;
				}
			}
			else if (bestLine.ObjID == MOD_ID_NONE)
			{
				objectHit.SnapType = ESnapType::CT_CORNERSNAP;
			}
			else
			{
				objectHit.Valid = false;
			}
		}
		// If the face hit hasn't been overridden by a point or edge, then see if it's on a set affordance plane,
		// in which case it might be a line segment whose distance should be snapped.
		else if (cursor.HasAffordanceSet() &&
			FVector::Parallel(cursor.AffordanceFrame.Normal, objectHit.Normal) &&
			FMath::IsNearlyZero(FPlane(cursor.AffordanceFrame.Origin, cursor.AffordanceFrame.Normal).PlaneDot(objectHit.Location), PLANAR_DOT_EPSILON))
		{
			SnapDistAlongAffordance(objectHit.Location, cursor.AffordanceFrame.Origin, FVector::ZeroVector);
		}
	}
	// Otherwise, map all of the point- and line-based (virtual) MOIs to locations that can be used to determine a hit result
	else
	{
		CurHitPointMOIs.Reset();
		CurHitPointLocations.Reset();
		CurHitLineMOIs.Reset();
		CurHitLineLocations.Reset();
		FPlane cullingPlane = GetCurrentCullingPlane();

		// TODO: we know this is inefficient, should replace with an interface that allows for optimization
		// (like not needing to iterate over every single object in the scene)
		auto& allObjects = Document->GetObjectInstances();
		for (auto* moi : allObjects)
		{
			// Use the same collision compatibility checks that the snapping view uses;
			// namely, whether collision is enabled and if the type of physics collision would've been allowed for direct raycasting
			ECollisionChannel objectCollisionType = UModumateTypeStatics::CollisionTypeFromObjectType(moi->GetObjectType());
			bool bObjectInMouseQuery = (mouseQueryBitfield & ECC_TO_BITFIELD(objectCollisionType)) != 0;

			if (moi && moi->IsCollisionEnabled() && moi->UseStructureDataForCollision() && bObjectInMouseQuery)
			{
				moi->RouteGetStructuralPointsAndLines(tempStructurePoints, tempStructureLines, false, false, cullingPlane);

				// Structural points and lines used for cursor hit collision are mutually exclusive
				if (tempStructureLines.Num() > 0)
				{
					for (auto line : tempStructureLines)
					{
						if (validateStructureLine(line))
						{
							CurHitLineMOIs.Add(moi);
							CurHitLineLocations.Add(TPair<FVector, FVector>(line.P1, line.P2));
						}
					}
				}
				else
				{
					for (auto point : tempStructurePoints)
					{
						if (validateStructurePoint(point))
						{
							CurHitPointMOIs.Add(moi);
							CurHitPointLocations.Add(point);
						}
					}
				}
			}
		}

		// Now find a virtual MOI hit result, preferring points over lines
		if (FindBestMousePointHit(CurHitPointLocations, mouseLoc, mouseDir, objectHitDist, bestVirtualHitIndex, bestVirtualHitDist))
		{
			objectHit.Valid = true;
			objectHit.Actor = CurHitPointMOIs[bestVirtualHitIndex]->GetActor();
			objectHit.Location = CurHitPointLocations[bestVirtualHitIndex].Point;
			objectHit.SnapType = ESnapType::CT_FACESELECT;
			objectHitDist = bestVirtualHitDist;
			if (objectHit.Actor == directHitActor)
			{
				objectHit.Normal = directHitNormal;
			}
		}
		else if (FindBestMouseLineHit(CurHitLineLocations, mouseLoc, mouseDir, objectHitDist, bestVirtualHitIndex, bestLineIntersection, bestVirtualHitDist))
		{
			objectHit.Valid = true;
			objectHit.Actor = CurHitLineMOIs[bestVirtualHitIndex]->GetActor();
			objectHit.Location = bestLineIntersection;
			objectHit.SnapType = ESnapType::CT_FACESELECT;
			objectHitDist = bestVirtualHitDist;
			if (objectHit.Actor == directHitActor)
			{
				objectHit.Normal = directHitNormal;
			}
		}
	}

	// TODO: we should disambiguate between candidate normals, but we'll want multiple affordances for a given snap point/edge before then

	return objectHit;
}

/*
Support function: given any of the hits (structural, object or sketch), if the user is holding shift, project either onto the custom affordance or the snapping span (axis aligned)
*/
FMouseWorldHitType AEditModelPlayerController::GetShiftConstrainedMouseHit(const FMouseWorldHitType &baseHit) const
{
	FMouseWorldHitType ret;

	if (!IsShiftDown())
	{
		EMPlayerState->SnappedCursor.ShiftLocked = false;
		return ret;
	}

	// If there are no snap points, then we require an active tool in order to get a shift-projected snap point.
	bool bRequireToolInUse = (UserSnapPoints.Num() == 0);

	if (UserSnapPoints.Num() == 0 && !ToolIsInUse() && InteractionHandle == nullptr)
	{
		return ret;
	}

	if (!baseHit.Valid)
	{
		return ret;
	}

	if (!EMPlayerState->SnappedCursor.ShiftLocked)
	{
		FVector origin = EMPlayerState->SnappedCursor.AffordanceFrame.Origin;
		FVector direction;

		switch (baseHit.SnapType)
		{
			case ESnapType::CT_WORLDSNAPX:
				direction = FVector::LeftVector;
				break;
			case ESnapType::CT_WORLDSNAPY:
				direction = FVector::ForwardVector;
				break;
			case ESnapType::CT_WORLDSNAPZ:
				direction = FVector::UpVector;
				break;
			case ESnapType::CT_USERPOINTSNAP:
			case ESnapType::CT_CUSTOMSNAPX:
				direction = EMPlayerState->SnappedCursor.AffordanceFrame.Tangent;
				break;
			case ESnapType::CT_CUSTOMSNAPY:
				direction = EMPlayerState->SnappedCursor.AffordanceFrame.Tangent;
				direction = FVector::CrossProduct(EMPlayerState->SnappedCursor.AffordanceFrame.Normal, EMPlayerState->SnappedCursor.AffordanceFrame.Tangent);
				break;
			case ESnapType::CT_CUSTOMSNAPZ:
				direction = EMPlayerState->SnappedCursor.AffordanceFrame.Normal;
				break;
			default:
				return ret;
		};
		EMPlayerState->SnappedCursor.ShiftLockOrigin = origin;
		EMPlayerState->SnappedCursor.ShiftLockDirection = direction;
		EMPlayerState->SnappedCursor.ShiftLocked = true;
	}

	if (EMPlayerState->SnappedCursor.ShiftLocked)
	{
		FMath::PointDistToLine(baseHit.Location, EMPlayerState->SnappedCursor.ShiftLockDirection, EMPlayerState->SnappedCursor.ShiftLockOrigin, ret.Location);
		ret.Valid = true;
	}

	return ret;
}

FLinearColor AEditModelPlayerController::GetSnapAffordanceColor(const FAffordanceLine &a)
{
	FVector p = (a.EndPoint - a.StartPoint).GetSafeNormal();
	if (FMath::IsNearlyEqual(fabsf(FVector::DotProduct(p, FVector(1, 0, 0))), 1.0f, 0.01f))
	{
		return FLinearColor::Red;
	}
	if (FMath::IsNearlyEqual(fabsf(FVector::DotProduct(p, FVector(0, 1, 0))), 1.0f, 0.01f))
	{
		return FLinearColor::Green;
	}
	if (FMath::IsNearlyEqual(fabsf(FVector::DotProduct(p, FVector(0, 0, 1))), 1.0f, 0.01f))
	{
		return FLinearColor::Blue;
	}
	return FLinearColor(FColorList::Magenta);
}

bool AEditModelPlayerController::AddSnapAffordance(const FVector &startLoc, const FVector &endLoc, const FLinearColor &overrideColor) const
{
	if (startLoc.Equals(endLoc))
	{
		return false;
	}

	FAffordanceLine affordance;
	affordance.StartPoint = startLoc;
	affordance.EndPoint = endLoc;
	affordance.Color = (overrideColor == FLinearColor::Transparent) ?
		GetSnapAffordanceColor(affordance) : overrideColor;
	affordance.Interval = 4.0f;
	EMPlayerState->AffordanceLines.Add(MoveTemp(affordance));

	return true;
}

void AEditModelPlayerController::AddSnapAffordancesToOrigin(const FVector &origin, const FVector &hitLocation) const
{
	FVector fromDim = hitLocation;
	FVector toDim = FVector(fromDim.X, fromDim.Y, origin.Z);

	AddSnapAffordance(fromDim, toDim);

	fromDim = toDim;
	toDim = origin;

	AddSnapAffordance(fromDim, toDim);
}

bool AEditModelPlayerController::GetCursorFromUserSnapPoint(const FTransform &snapPoint, FVector &outCursorPosFlat, FVector &outHeightDelta) const
{
	FVector activeSnapPointPos = snapPoint.GetLocation();
	FQuat activeSnapPointRot = snapPoint.GetRotation();
	FVector cursorPos = EMPlayerState->SnappedCursor.WorldPosition;

	FVector pointToCursorDelta = cursorPos - activeSnapPointPos;
	float pointToCursorDist = pointToCursorDelta.Size();

	if (!pointToCursorDelta.IsZero())
	{
		FVector snapPointUp = activeSnapPointRot.GetAxisZ();
		outHeightDelta = snapPointUp * (pointToCursorDelta | snapPointUp);
		outCursorPosFlat = activeSnapPointPos + pointToCursorDelta - outHeightDelta;

		return true;
	}

	return false;
}

bool AEditModelPlayerController::HasUserSnapPointAtPos(const FVector &snapPointPos, float tolerance) const
{
	for (const FTransform &userSnapPoint : UserSnapPoints)
	{
		if (userSnapPoint.GetLocation().Equals(snapPointPos, tolerance))
		{
			return true;
		}
	}

	return false;
}

void AEditModelPlayerController::UpdateUserSnapPoint()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ModumateUpdateSnaps);

	FTransform activeUserSnapPoint;
	FVector cursorPosFlat, cursorHeightDelta;
	if (GetActiveUserSnapPoint(activeUserSnapPoint) && GetCursorFromUserSnapPoint(activeUserSnapPoint, cursorPosFlat, cursorHeightDelta))
	{
		FVector userSnapPointPos = activeUserSnapPoint.GetLocation();

		// TODO: non-horizontal dimensions
		AddSnapAffordance(cursorPosFlat, EMPlayerState->SnappedCursor.WorldPosition);

		FLinearColor userPointSnapColor = (EMPlayerState->SnappedCursor.SnapType == ESnapType::CT_NOSNAP) ?
			FLinearColor::Black : FLinearColor::Transparent;

		if (EMPlayerState->SnappedCursor.HasProjectedPosition)
		{
			AddSnapAffordancesToOrigin(EMPlayerState->SnappedCursor.WorldPosition, EMPlayerState->SnappedCursor.ProjectedPosition);
			userPointSnapColor = FLinearColor::Transparent;
		}

		AddSnapAffordance(userSnapPointPos, cursorPosFlat, userPointSnapColor);
	}
	// TODO: do this exactly when mouse mode changes, rather than polling for the change
	else if (EMPlayerState->SnappedCursor.MouseMode != EMouseMode::Location)
	{
		ClearUserSnapPoints();
	}
}

void AEditModelPlayerController::SetSelectionMode(ESelectObjectMode NewSelectionMode)
{
	if (SelectionMode != NewSelectionMode)
	{
		SelectionMode = NewSelectionMode;

		switch (SelectionMode)
		{
		case ESelectObjectMode::DefaultObjects:
			CurrentMouseCursor = EMouseCursor::Default;
			break;
		case ESelectObjectMode::Advanced:
			CurrentMouseCursor = EMouseCursor::Custom;
			break;
		default:
			break;
		}

		EMPlayerState->UpdateObjectVisibilityAndCollision();
	}
}

bool AEditModelPlayerController::ToggleGravityPawn()
{
	// Get required pawns
	APlayerCameraManager* camManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	APawn* currentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	auto* gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	if (!(camManager && EMPlayerPawn && currentPawn && gameMode && gameMode->ToggleGravityPawnClass))
	{
		return false;
	}

	FVector possessLocation = camManager->GetCameraLocation();
	FRotator possessRotation = camManager->GetCameraRotation();

	// Check which pawn is the user currently in. If it's in EMPlayerPawn, should switch to WalkAroundPawn
	if (currentPawn == EMPlayerPawn)
	{
		FRotator yawRotOnly = FRotator(0.f, possessRotation.Yaw, 0.f);
		if (EMToggleGravityPawn == nullptr)
		{
			FActorSpawnParameters spawnParam;
			spawnParam.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			EMToggleGravityPawn = GetWorld()->SpawnActor<AEditModelToggleGravityPawn>(gameMode->ToggleGravityPawnClass, possessLocation, yawRotOnly, spawnParam);
		}
		EMToggleGravityPawn->SetActorLocationAndRotation(possessLocation, yawRotOnly, true);
		Possess(EMToggleGravityPawn);
		EMToggleGravityPawn->SetActorEnableCollision(true);
		SetControlRotation(possessRotation);
	}
	else if (currentPawn == EMToggleGravityPawn)
	{
		Possess(EMPlayerPawn);
		EMToggleGravityPawn->SetActorEnableCollision(false);
	}
	else
	{
		ensureAlwaysMsgf(false, TEXT("Unknown pawn possessed!"));
		return false;
	}

	static const FString analyticsEventName(TEXT("ToggleGravity"));
	UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::View, analyticsEventName);

	return true;
}

void AEditModelPlayerController::SetAlwaysShowGraphDirection(bool NewShow)
{
	bAlwaysShowGraphDirection = NewShow;
	for(auto curType : UModumateTypeStatics::GetObjectTypeWithDirectionIndicator())
	{
		for (auto curObj : Document->GetObjectsOfType(curType))
		{
			curObj->MarkDirty(EObjectDirtyFlags::Visuals);
		}
	}
}

void AEditModelPlayerController::ProjectPermissionsChangedHandler()
{
	if (EditModelUserWidget && EMPlayerState)
	{
		if (EMPlayerState->ReplicatedProjectPermissions.CanEdit)
		{
			EditModelUserWidget->ToggleViewOnlyBadge(false);
		}
		else
		{
			EditModelUserWidget->ToggleViewOnlyBadge(true);
		}
	}
}

void AEditModelPlayerController::OnToggledProjectSystemMenu(ESlateVisibility NewVisibility)
{
	// When showing the project system menu in multiplayer, use the upload time & hash from the server for the Save button.
	auto* world = GetWorld();
	auto* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	if (EditModelUserWidget && EditModelUserWidget->ProjectSystemMenu &&
		EditModelUserWidget->ProjectSystemMenu->IsVisible() &&
		EditModelUserWidget->ProjectSystemMenu->ButtonSaveProject)
	{
		if (IsNetMode(NM_Client) && gameState && gameState->Document)
		{
			double uploadAge = (FDateTime::UtcNow() - gameState->LastUploadTime).GetTotalSeconds();
			FNumberFormattingOptions timeFormatOpts;
			timeFormatOpts.MaximumFractionalDigits = 0;

			FText saveFormatText = (gameState->Document->GetLatestVerifiedDocHash() == gameState->LastUploadedDocHash) ?
				LOCTEXT("OnlineSavedFormat", "Up to date.\nAuto-saved by the server {0} seconds ago.") :
				LOCTEXT("OnlineUnsavedFormat", "Auto-saved by the server {0} seconds ago.");
			FText saveText = FText::Format(saveFormatText, FText::AsNumber(uploadAge, &timeFormatOpts));

			EditModelUserWidget->ProjectSystemMenu->ButtonSaveProject->OverrideTooltipText = saveText;
		}
#if UE_BUILD_SHIPPING
		else
		{
			{
				EditModelUserWidget->ProjectSystemMenu->ButtonSaveProject->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
#endif
	}
}

void AEditModelPlayerController::ConnectVoiceClient(AModumateVoice* UsersVoiceClient)
{
	if (IsNetMode(NM_Client) && UsersVoiceClient)
	{
		UE_LOG(LogTemp, Log, TEXT("Voice Chat Connecting"))

		VoiceClient = UsersVoiceClient;

		auto gameInstance = GetGameInstance<UModumateGameInstance>();

		FString name = gameInstance->GetAccountManager()->GetUserInfo().ID;
		FString channel = gameInstance->GetCloudConnection()->GetCloudProjectPageURL() + TEXT("/") + gameInstance->DocumentProjectID;

		channel = channel.Replace(TEXT(":"), TEXT("_"));
		channel = channel.Replace(TEXT("/"), TEXT("_"));
		channel = channel.Replace(TEXT("."), TEXT("_"));

		VoiceClient->Connect(name, channel);

		VoiceConnectedEvent.Broadcast();
	}
}

void AEditModelPlayerController::ConnectTextChatClient(AModumateTextChat* UsersTextChatClient)
{
	if (UsersTextChatClient)
	{
		UE_LOG(LogTemp, Log, TEXT("Text Chat Connecting"))

		TextChatClient = UsersTextChatClient;

		if (IsNetMode(NM_Client))
		{
			auto gameInstance = GetGameInstance<UModumateGameInstance>();
			const FString id = gameInstance->GetAccountManager()->GetUserInfo().ID;
			TextChatClient->Connect(id);
			TextChatConnectedEvent.Broadcast();
		}
		
	}
}

void AEditModelPlayerController::SetCurrentCullingCutPlane(int32 ObjID /*= MOD_ID_NONE*/, bool bRefreshMenu /*= true*/)
{
	// Stop previous culling cutplane from culling
	AModumateObjectInstance* previousCullingMoi = Document->GetObjectById(CurrentCullingCutPlaneID);
	if (previousCullingMoi)
	{
		AMOICutPlane* previousCutPlane = Cast<AMOICutPlane>(previousCullingMoi);
		if (previousCutPlane)
		{
			previousCutPlane->SetIsCulling(false);
		}
	}

	// Update the menu
	CurrentCullingCutPlaneID = ObjID;

	// Ask new culling cutplane to cull
	AModumateObjectInstance* newCullingMoi = Document->GetObjectById(CurrentCullingCutPlaneID);
	if (newCullingMoi)
	{
		AMOICutPlane* newCutPlane = Cast<AMOICutPlane>(newCullingMoi);
		if (newCutPlane)
		{
			newCutPlane->SetIsCulling(true);
		}
	}
	UpdateCutPlaneCullingMaterialInst(CurrentCullingCutPlaneID);
	if (bRefreshMenu)
	{
		EditModelUserWidget->CutPlaneMenu->UpdateCutPlaneMenuBlocks();
	}
}

void AEditModelPlayerController::UpdateCutPlaneCullingMaterialInst(int32 ObjID /*= MOD_ID_NONE*/)
{
	FVector planePos = FVector::ZeroVector;
	FVector rotAxis = FVector::ZeroVector;
	float enableValue = 0.f;

	AModumateObjectInstance* moi = Document->GetObjectById(ObjID);
	if (moi && moi->GetObjectType() == EObjectType::OTCutPlane)
	{
		planePos = moi->GetLocation();
		rotAxis = moi->GetNormal() * -1.f;
		enableValue = 1.f;
	}

	if (CutPlaneCullingMaterialCollection)
	{
		UMaterialParameterCollectionInstance* cullingInst = GetWorld()->GetParameterCollectionInstance(CutPlaneCullingMaterialCollection);
		cullingInst->SetVectorParameterValue(TEXT("Position"), planePos);
		cullingInst->SetVectorParameterValue(TEXT("Axis"), rotAxis);
		cullingInst->SetScalarParameterValue(TEXT("EnableValue"), enableValue);
	}
}

void AEditModelPlayerController::ToggleAllCutPlanesColor(bool bEnable)
{
	TArray<AModumateObjectInstance*> mois = Document->GetObjectsOfType(EObjectType::OTCutPlane);
	for (auto moi : mois)
	{
		AMOICutPlane* cp = Cast<AMOICutPlane>(moi);
		if (cp)
		{
			cp->SetHUDDwgDrafting(bEnable);
			bool bOutVisible; bool bOutCollisionEnabled;
			cp->GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled);
		}
	}
}

FPlane AEditModelPlayerController::GetCurrentCullingPlane() const
{
	auto cullingCutPlaneMOI = (Document && (CurrentCullingCutPlaneID != MOD_ID_NONE)) ? Document->GetObjectById(CurrentCullingCutPlaneID) : nullptr;
	return (cullingCutPlaneMOI && ensure(cullingCutPlaneMOI->GetObjectType() == EObjectType::OTCutPlane)) ?
		FPlane(cullingCutPlaneMOI->GetLocation(), cullingCutPlaneMOI->GetNormal()) :
		FPlane(ForceInitToZero);
}

void AEditModelPlayerController::ToggleDrawingDesigner(bool bEnable)
{
	EditModelUserWidget->DrawingDesigner->SetVisibility(bEnable ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void AEditModelPlayerController::CapabilityReady(AModumateCapability* Capability)
{
	//Route capability to the correct call
	if (Capability->IsA<AModumateVoice>())
	{
		ConnectVoiceClient(Cast<AModumateVoice>(Capability));
	}

	if (Capability->IsA<AModumateTextChat>())
	{
		ConnectTextChatClient(Cast<AModumateTextChat>(Capability));
	}
}

void AEditModelPlayerController::HandleUndo()
{
	UModumateDocument* doc = GetDocument();

	if (doc && !doc->IsPreviewingDeltas())
	{
		doc->Undo(GetWorld());
		EMPlayerState->ValidateSelectionsAndView();
	}

	if (EditModelUserWidget->IsBIMDesingerActive())
	{
		EditModelUserWidget->BIMDesigner->RefreshNodes();
	}
}

void AEditModelPlayerController::HandleRedo()
{
	UModumateDocument* doc = GetDocument();

	if (doc && !doc->IsPreviewingDeltas())
	{
		doc->Redo(GetWorld());
		EMPlayerState->ValidateSelectionsAndView();
	}

	if (EditModelUserWidget->IsBIMDesingerActive())
	{
		EditModelUserWidget->BIMDesigner->RefreshNodes();
	}
}

#undef LOCTEXT_NAMESPACE
