// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelPlayerController.h"

#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/EditableTextBox.h"
#include "Database/ModumateObjectDatabase.h"
#include "Drafting/APDFLLib.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Engine/SceneCapture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateStats.h"
#include "ModumateCore/ModumateThumbnailHelpers.h"
#include "ModumateCore/PlatformFunctions.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateCloudConnection.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/DynamicIconGenerator.h"
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
#include "UI/AdjustmentHandleWidget.h"
#include "UI/DimensionActor.h"
#include "UI/DimensionManager.h"
#include "UI/EditModelUserWidget.h"


// Tools
#include "ToolsAndAdjustments/Tools/EditModelCabinetTool.h"
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

static Modumate::PDF::PDFResult PDFLibrary;

const FString AEditModelPlayerController::InputTelemetryDirectory(TEXT("Telemetry"));

using namespace Modumate;

/*
* Constructor
*/
AEditModelPlayerController::AEditModelPlayerController()
	: APlayerController()
	, Document(nullptr)
	, SnappingView(nullptr)
	, UserSnapAutoCreationElapsed(0.0f)
	, UserSnapAutoCreationDelay(0.4f)
	, UserSnapAutoCreationDuration(0.75f)
	, EMPlayerState(nullptr)
	, TestBox(ForceInitToZero)
	, ShowingModalDialog(false)
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

	// Init the PDF library once the application has actually started
	PDFLibrary = Modumate::PDF::InitLibrary();

	// Cache our raycasting parameters
	static const FName HandleTraceTag(TEXT("HandleTrace"));
	HandleTraceQueryParams = FCollisionQueryParams(HandleTraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerController), false);
	HandleTraceObjectQueryParams = FCollisionObjectQueryParams(COLLISION_HANDLE);

	// Load all of our hardware cursors here, so they can be used by the base PlayerController
	UWorld *world = GetWorld();
	UGameViewportClient *gameViewport = world ? world->GetGameViewport() : nullptr;
	if (gameViewport)
	{
		static const FName SelectCursorPath(TEXT("NonUAssets/Cursors/Cursor_select_basic"));
		static const FName AdvancedCursorPath(TEXT("NonUAssets/Cursors/Cursor_select_advanced"));
		static const FVector2D CursorHotspot(0.28125f, 0.1875f);
		gameViewport->SetHardwareCursor(EMouseCursor::Default, SelectCursorPath, CursorHotspot);
		gameViewport->SetHardwareCursor(EMouseCursor::Custom, AdvancedCursorPath, CursorHotspot);
	}

	// Assign our cached casted AEditModelPlayerState, and its cached casted pointer to us,
	// immediately after it's created.
	AEditModelGameMode *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	if ((gameMode != nullptr) && ensureAlways(PlayerState != nullptr))
	{
		EMPlayerState = Cast<AEditModelPlayerState>(PlayerState);
		EMPlayerState->EMPlayerController = this;
	}
}

/*
* Unreal Event Handlers
*/

void AEditModelPlayerController::BeginPlay()
{
	Super::BeginPlay();

	AEditModelGameState* gameState = GetWorld()->GetGameState<AEditModelGameState>();
	Document = gameState->Document;

	EMPlayerPawn = Cast<AEditModelPlayerPawn>(GetPawn());
	if (ensure(EMPlayerPawn))
	{
		SetViewTargetWithBlend(EMPlayerPawn);
	}

	SnappingView = new FModumateSnappingView(Document, this);

	CreateTools();
	SetToolMode(EToolMode::VE_SELECT);

	auto* playerHUD = GetEditModelHUD();
	playerHUD->Initialize();
	HUDDrawWidget = playerHUD->HUDDrawWidget;

	// If we have a crash recovery, load that 
	// otherwise if the game mode started with a pending project (if it came from the command line, for example) then load it immediately. 

	FString fileLoadPath;
	bool bSetAsCurrentProject = true;
	bool bAddToRecents = true;
	bool bEnableAutoSave = true;

	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();

	UGameViewportClient* viewportClient = gameInstance->GetGameViewportClient();
	if (viewportClient)
	{
		viewportClient->OnWindowCloseRequested().BindUObject(this, &AEditModelPlayerController::CheckSaveModel);
	}

	AEditModelGameMode *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();

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
		fileLoadPath = gameMode->PendingProjectPath;
		bSetAsCurrentProject = true;
		bAddToRecents = !gameMode->bPendingProjectIsTutorial;
		bEnableAutoSave = !gameMode->bPendingProjectIsTutorial;
	}

	if (!fileLoadPath.IsEmpty())
	{
		LoadModelFilePath(fileLoadPath, bSetAsCurrentProject, bAddToRecents, bEnableAutoSave);
	}
	else
	{
		NewModel(false);
	}

	EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	FString lockFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);
	FFileHelper::SaveStringToFile(TEXT("lock"), *lockFile);

	GetWorldTimerManager().SetTimer(ControllerTimer, this, &AEditModelPlayerController::OnControllerTimer, 10.0f, true, 0.0f);

	TimeOfLastAutoSave = FDateTime::Now();

	// Create icon generator in the farthest corner of the universe
	// TODO: Ideally the scene capture comp should capture itself only, but UE4 lacks that feature, for now...
	DynamicIconGenerator = GetWorld()->SpawnActor<ADynamicIconGenerator>(DynamicIconGeneratorClass);
	if (ensureAlways(DynamicIconGenerator))
	{
		DynamicIconGenerator->SetActorLocation(FVector(-100000.f, -100000.f, -100000.f));
	}
	EditModelUserWidget = CreateWidget<UEditModelUserWidget>(this, EditModelUserWidgetClass);
	if (ensureAlways(EditModelUserWidget))
	{
		EditModelUserWidget->AddToViewport(1);
	}

#if !UE_BUILD_SHIPPING
	// Now that we've registered our commands, register them in the UE4 console's autocompletion list
	UModumateConsole *console = Cast<UModumateConsole>(GEngine->GameViewport ? GEngine->GameViewport->ViewportConsole : nullptr);
	if (console)
	{
		console->BuildRuntimeAutoCompleteList(true);
	}
#endif

	// Make a clean telemetry directory, also cleans up on EndPlay 
	FString path = FModumateUserSettings::GetLocalTempDir() / InputTelemetryDirectory;
	IFileManager::Get().DeleteDirectory(*path, false, true);
	IFileManager::Get().MakeDirectory(*path);
}

bool AEditModelPlayerController::StartTelemetrySession(bool bRecordInput)
{
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance->GetAccountManager().Get()->ShouldRecordTelemetry())
	{
		return false;
	}

	EndTelemetrySession();

	TelemetrySessionKey = FGuid::NewGuid();
	SessionStartTime = FDateTime::Now();

	if (!bRecordInput)
	{
		return true;
	}

	if (InputAutomationComponent->BeginRecording())
	{
		TimeOfLastUpload = FDateTime::Now();

		const auto* projectSettings = GetDefault<UGeneralProjectSettings>();
		if (!ensureAlways(projectSettings != nullptr))
		{
			return false;
		}
		const FString& projectVersion = projectSettings->ProjectVersion;

		TSharedPtr<FModumateCloudConnection> Cloud = gameInstance->GetCloudConnection();

		if (Cloud.IsValid())
		{
			Cloud->CreateReplay(TelemetrySessionKey.ToString(), *projectVersion, [](bool bSuccess, const TSharedPtr<FJsonObject>& Response) {
				UE_LOG(LogTemp, Log, TEXT("Created Successfully"));

			}, [](int32 code, const FString& error) {
				UE_LOG(LogTemp, Error, TEXT("Error: %s"), *error);
			});

			return true;
		}
	}

	return false;
}

bool AEditModelPlayerController::EndTelemetrySession()
{
	if (InputAutomationComponent->IsRecording())
	{
		UploadInputTelemetry();
		InputAutomationComponent->EndRecording(false);
	}

	// If we don't have a session key, there's nothing to record
	if (TelemetrySessionKey.IsValid())
	{
		FTimespan sessionTime = FDateTime::Now() - SessionStartTime;
		UModumateAnalyticsStatics::RecordSessionDuration(this, sessionTime);
		TelemetrySessionKey.Invalidate();
	}

	return true;
}

void AEditModelPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	FString lockFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*lockFile);
	GetWorldTimerManager().ClearTimer(ControllerTimer);

	EndTelemetrySession();

	// Clean up input telemetry files
	FString path = FModumateUserSettings::GetLocalTempDir() / InputTelemetryDirectory;
	IFileManager::Get().DeleteDirectory(*path,false,true);

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
	float v = 0.f;
	// Most cases input is in imperial unit, unless is specific handle or tool mode
	if (curToolMode == EToolMode::VE_ROTATE || // Rotate tool uses degree
		(InteractionHandle && !InteractionHandle->HasDistanceTextInput()))
	{
		UModumateDimensionStatics::TryParseInputNumber(Text.ToString(), v);
	}
	else
	{
		v = UModumateDimensionStatics::StringToMetric(Text.ToString(), true);
	}

	const float MAX_DIMENSION = 525600 * 12 * 2.54;

	if (v != 0.0f && v <= MAX_DIMENSION)
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
			if (InteractionHandle->HandleInputNumber(v))
			{
				InteractionHandle = nullptr;
				return;
			}
		}
		else if (CurrentTool && CurrentTool->IsInUse())
		{
			CurrentTool->HandleInputNumber(v);
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
	RegisterTool(CreateTool<UStructureLineTool>());
	RegisterTool(CreateTool<UDrawingTool>());
	RegisterTool(CreateTool<UGraph2DTool>());
	RegisterTool(CreateTool<USurfaceGraphTool>());
	RegisterTool(CreateTool<UPanelTool>());
	RegisterTool(CreateTool<UMullionTool>());
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
	if (!CheckUserPlanAndPermission(EModumatePermission::Save))
	{
		return false;
	}

	if (EMPlayerState->LastFilePath.IsEmpty())
	{
		return SaveModelAs();
	}
	return SaveModelFilePath(EMPlayerState->LastFilePath);
}

bool AEditModelPlayerController::SaveModelAs()
{
	if (!CheckUserPlanAndPermission(EModumatePermission::Save))
	{
		return false;
	}

	if (EMPlayerState->ShowingFileDialog)
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
	if (Modumate::PlatformFunctions::GetSaveFilename(filename, INDEX_MODFILE))
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

	// Try to capture a thumbnail for the project	
	CaptureProjectThumbnail();
	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	if (gameState->Document->Save(GetWorld(), filepath, true))
	{
		EMPlayerState->LastFilePath = filepath;
		Modumate::PlatformFunctions::ShowMessageBox(*FString::Printf(TEXT("Saved as %s"),*EMPlayerState->LastFilePath), TEXT("Save Model"), Modumate::PlatformFunctions::Okay);

		static const FString eventName(TEXT("SaveDocument"));
		UModumateAnalyticsStatics::RecordEventSimple(this, UModumateAnalyticsStatics::EventCategorySession, eventName);
		return true;
	}
	else
	{
		Modumate::PlatformFunctions::ShowMessageBox(*FString::Printf(TEXT("COULD NOT SAVE %s"), *EMPlayerState->LastFilePath), TEXT("Save Model"), Modumate::PlatformFunctions::Okay);
		return false;
	}
}

bool AEditModelPlayerController::LoadModel()
{
	if (EMPlayerState->ShowingFileDialog)
	{
		return false;
	}

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

	if (Modumate::PlatformFunctions::GetOpenFilename(filename))
	{
		bLoadSuccess = LoadModelFilePath(filename, true, true, true);
	}

	EMPlayerState->ShowingFileDialog = false;
	return bLoadSuccess;
}

bool AEditModelPlayerController::LoadModelFilePath(const FString &filename, bool bSetAsCurrentProject, bool bAddToRecents, bool bEnableAutoSave)
{
	EndTelemetrySession();
	EMPlayerState->OnNewModel();

	bool bLoadSuccess = Document->Load(GetWorld(), filename, bSetAsCurrentProject, bAddToRecents);

	if (bLoadSuccess)
	{
		static const FString LoadDocumentEventName(TEXT("LoadDocument"));
		UModumateAnalyticsStatics::RecordEventSimple(this, UModumateAnalyticsStatics::EventCategorySession, LoadDocumentEventName);

		// TODO: always record input, and remove this flag, when we can upload the loaded document as the start of the input telemetry log
		StartTelemetrySession(false);

		TimeOfLastAutoSave = FDateTime::Now();
		bWantAutoSave = false;
		bCurProjectAutoSaves = bEnableAutoSave;
	}

	if (bLoadSuccess && bSetAsCurrentProject)
	{
		EMPlayerState->LastFilePath = filename;
	}
	else
	{
		EMPlayerState->LastFilePath.Empty();
	}

	return bLoadSuccess;
}

bool AEditModelPlayerController::CheckSaveModel()
{
	if (Document->IsDirty())
	{
		Modumate::PlatformFunctions::EMessageBoxResponse resp = Modumate::PlatformFunctions::ShowMessageBox(TEXT("Save current model?"), TEXT("Save"), Modumate::PlatformFunctions::YesNoCancel);
		if (resp == Modumate::PlatformFunctions::Yes)
		{
			if (!SaveModelAs())
			{
				return false;
			}
		}
		if (resp == Modumate::PlatformFunctions::Cancel)
		{
			return false;
		}
	}
	return true;
}

void AEditModelPlayerController::NewModel(bool bShouldCheckForSave)
{
	if (!EMPlayerState->ShowingFileDialog)
	{
		if (bShouldCheckForSave && !CheckSaveModel())
		{
			return;
		}

		EndTelemetrySession();

		static const FString NewDocumentEventName(TEXT("NewDocument"));
		UModumateAnalyticsStatics::RecordEventSimple(this, UModumateAnalyticsStatics::EventCategorySession, NewDocumentEventName);

		EMPlayerState->OnNewModel();
		Document->MakeNew(GetWorld());

		TimeOfLastAutoSave = FDateTime::Now();
		bWantAutoSave = false;
		bCurProjectAutoSaves = true;

		StartTelemetrySession(true);
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
		FVector captureOrigin = CalculateViewLocationForSphere(projectBounds, captureComp->GetForwardVector(), captureMaxAspect, captureComp->FOVAngle);
		captureComp->GetOwner()->SetActorLocation(captureOrigin);

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

bool AEditModelPlayerController::GetScreenshotFileNameWithDialog(FString &filename)
{
	if (EMPlayerState->ShowingFileDialog)
	{
		return false;
	}

	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	EMPlayerState->ShowingFileDialog = true;

	// Open the file dialog
	bool bChoseFile = false;
	if (Modumate::PlatformFunctions::GetSaveFilename(filename, INDEX_PNGFILE))
	{
		bChoseFile = true;
	}

	EMPlayerState->ShowingFileDialog = false;

	return bChoseFile;
}

void AEditModelPlayerController::TrySavePDF()
{
	OnSavePDF();
}

bool AEditModelPlayerController::CheckUserPlanAndPermission(EModumatePermission Permission)
{
#if WITH_EDITOR
	return true;
#endif
	// TODO: Check if user is still login, refresh and request permission status
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (ensure(gameInstance) && gameInstance->GetAccountManager().Get()->HasPermission(Permission))
	{
		return true;
	}
	// TODO: Check if user is on paid plan
	EditModelUserWidget->ShowAlertFreeAccountDialog();
	return false;
}

bool AEditModelPlayerController::OnSavePDF()
{
	if (!CheckUserPlanAndPermission(EModumatePermission::Export))
	{
		return false;
	}

	if (EMPlayerState->ShowingFileDialog)
	{
		return false;
	}

	if (ToolIsInUse())
	{
		AbortUseTool();
	}


	if (PDFLibrary.ErrorCode != Modumate::EDrawError::ErrorNone)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString("Could Not Init PDF Library")));
		return false;
	}

	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance)
	{
		return false;
	}

	static const FText dialogTitle = FText::FromString(FString(TEXT("PDF Creation")) );
	if (!gameInstance->IsloggedIn())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(FString(TEXT("You must be logged in to export to a PDF file") )),
			&dialogTitle);
		return false;
	}

	EMPlayerState->ShowingFileDialog = true;

	FString filename;
	bool ret = true;

	if (Modumate::PlatformFunctions::GetSaveFilename(filename, INDEX_PDFFILE))
	{
		FString root, libDir;
#if WITH_EDITOR
		root = FPaths::GetPath(FPaths::GetProjectFilePath());
		libDir = FPaths::Combine(root, L"APDFL/DLLs");
#else
		root = FPaths::ProjectDir();
		libDir = FPaths::Combine(root, L"Binaries/Win64");
#endif
		libDir = FPaths::ConvertRelativePathToFull(libDir);


		if (!Document->ExportPDF(GetWorld(), *filename, FVector::ZeroVector, FVector::ZeroVector))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString("PDF Export Failed")));
			ret = false;
		}
	}

	EMPlayerState->ShowingFileDialog = false;
	return ret;
}

bool AEditModelPlayerController::OnCreateDwg()
{
	if (!CheckUserPlanAndPermission(EModumatePermission::Export))
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

	static const FText dialogTitle = FText::FromString(FString(TEXT("DWG Creation")));
	if (!gameInstance->IsloggedIn())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(FString(TEXT("You must be logged in to export to DWG files")) ),
			&dialogTitle);
		return false;
	}

	EMPlayerState->ShowingFileDialog = true;

	FString filename;
	if (Modumate::PlatformFunctions::GetSaveFilename(filename, INDEX_DWGFILE))
	{
		EMPlayerState->ShowingFileDialog = false;

		if (!Document->ExportDWG(GetWorld(), *filename))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString(TEXT("DWG Creation Failed")) ),
				&dialogTitle);
			retValue = false;
		}

	}
	else
	{
		EMPlayerState->ShowingFileDialog = false;
	}

	return retValue;
}

void AEditModelPlayerController::DeleteActionDefault()
{
	ModumateCommand(
		FModumateCommand(Commands::kDeleteSelectedObjects)
		.Param(Parameters::kIncludeConnected, true)
	);
}

void AEditModelPlayerController::DeleteActionOnlySelected()
{
	ModumateCommand(
		FModumateCommand(Commands::kDeleteSelectedObjects)
		.Param(Parameters::kIncludeConnected, false)
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
	else if (EMPlayerState->SelectedObjects.Num() > 0)
	{
		DeselectAll();
		return true;
	}
	else if (EMPlayerState->ViewGroupObject)
	{
		SetViewGroupObject(EMPlayerState->ViewGroupObject->GetParentObject());
	}
	else if (CurrentTool && (CurrentTool->GetToolMode() != EToolMode::VE_SELECT))
	{
		SetToolMode(EToolMode::VE_SELECT);
		EMPlayerState->SnappedCursor.ClearAffordanceFrame();
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
	return IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
}

void AEditModelPlayerController::OnControllerTimer()
{
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());

	FTimespan ts(FDateTime::Now().GetTicks() - TimeOfLastAutoSave.GetTicks());

	if (ts.GetTotalSeconds() > gameInstance->UserSettings.AutoBackupFrequencySeconds && gameInstance->UserSettings.AutoBackup)
	{
		bWantAutoSave = gameInstance->UserSettings.AutoBackup && bCurProjectAutoSaves;
	}

	ts = FTimespan(FDateTime::Now().GetTicks() - TimeOfLastUpload.GetTicks());
	if (ts.GetTotalSeconds() > gameInstance->UserSettings.TelemetryUploadFrequencySeconds)
	{
		bWantTelemetryUpload = gameInstance->GetAccountManager().Get()->ShouldRecordTelemetry();
	}

	gameInstance->GetCloudConnection()->Tick();
}

DECLARE_CYCLE_STAT(TEXT("Edit tick"), STAT_ModumateEditTick, STATGROUP_Modumate)

bool AEditModelPlayerController::UploadInputTelemetry() const
{
	if (!InputAutomationComponent->IsRecording())
	{
		return false;
	}

	FString path = FModumateUserSettings::GetLocalTempDir() / InputTelemetryDirectory;
	FString  cacheFile =  path / TelemetrySessionKey.ToString() + FEditModelInputLog::LogExtension;
	if (!InputAutomationComponent->SaveInputLog(cacheFile))
	{
		return false;
	}

	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	TSharedPtr<FModumateCloudConnection> Cloud = gameInstance->GetCloudConnection();

	if (Cloud.IsValid())
	{
		Cloud->UploadReplay(TelemetrySessionKey.ToString(), *cacheFile, [](bool bSuccess, const TSharedPtr<FJsonObject>& Response) {
			UE_LOG(LogTemp, Log, TEXT("Uploaded Successfully"));
		}, [](int32 code, const FString& error) {
			UE_LOG(LogTemp, Error, TEXT("Error: %s"), *error);
		});

		return true;
	}
	return false;
}

void AEditModelPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SCOPE_CYCLE_COUNTER(STAT_ModumateEditTick);

	// Don't perform hitchy functions while tools or handles are in use
	if (InteractionHandle == nullptr && !ToolIsInUse())
	{
		if (bWantTelemetryUpload)
		{
			TimeOfLastUpload = FDateTime::Now();
			bWantTelemetryUpload = false;
			if (TelemetrySessionKey.IsValid())
			{
				UploadInputTelemetry();
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
			gameState->Document->Save(GetWorld(), newFile, false);

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

	if (EMPlayerState->ShowDocumentDebug)
	{
		Document->DisplayDebugInfo(GetWorld());

		AEditModelGameMode *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
		TArray<FString> dbstrs = gameMode->ObjectDatabase->GetDebugInfo();
		for (auto &str : dbstrs)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, str, false);
		}

		for (auto &sob : EMPlayerState->SelectedObjects)
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
}

void AEditModelPlayerController::TickInput(float DeltaTime)
{
	// Skip input updates if there are no OS windows with active input focus
	auto viewportClient = Cast<UModumateViewportClient>(GetWorld()->GetGameViewport());
	if ((viewportClient == nullptr) || !viewportClient->AreWindowsActive())
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
			if (!InteractionHandle->UpdateUse())
			{
				InteractionHandle->EndUse();
				InteractionHandle = nullptr;
			}
		}
		else if (CurrentTool != nullptr)
		{
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

	if (bResetFocusToGameViewport)
	{
		bResetFocusToGameViewport = false;
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

FVector AEditModelPlayerController::CalculateViewLocationForSphere(const FSphere &TargetSphere, const FVector &ViewVector, float AspectRatio, float FOV)
{
	float captureHalfFOV = 0.5f * FMath::DegreesToRadians(FOV);
	float captureDistance = AspectRatio * TargetSphere.W / FMath::Tan(captureHalfFOV);
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

void AEditModelPlayerController::SetObjectSelected(const AModumateObjectInstance *ob, bool selected)
{
	ModumateCommand(
		FModumateCommand(Commands::kSelectObject)
		.Param(Parameters::kObjectID, ob->ID)
		.Param(Parameters::kSelected, selected)
	);
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

void AEditModelPlayerController::HandleLeftMouseButton_Implementation(bool bPressed)
{
	// TODO: handle input logic here, see OnLButtonDown, OnLButtonUp
}

void AEditModelPlayerController::SelectAll()
{
	ModumateCommand(FModumateCommand(Commands::kSelectAll));
}

void AEditModelPlayerController::SelectInverse()
{
	ModumateCommand(FModumateCommand(Commands::kSelectInverse));
}

void AEditModelPlayerController::DeselectAll()
{
	ModumateCommand(FModumateCommand(Commands::kDeselectAll));
}

bool AEditModelPlayerController::SelectObjectById(int32 ObjectID)
{
	const AModumateObjectInstance *moi = Document->GetObjectById(ObjectID);
	if (moi == nullptr)
	{
		return false;
	}

	DeselectAll();
	SetObjectSelected(moi, true);

	return true;
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
			FModumateCommand(Commands::kViewGroupObject)
			.Param(Parameters::kObjectID, ob->ID)
		);
	}
	else
	{
		ModumateCommand(
			FModumateCommand(Commands::kViewGroupObject)
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
	case EToolMode::VE_WALL:
	case EToolMode::VE_FLOOR:
	case EToolMode::VE_CEILING:
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_RAIL:
	case EToolMode::VE_STAIR:
	case EToolMode::VE_RECTANGLE:
		MOITraceObjectQueryParams = FCollisionObjectQueryParams(COLLISION_META_MOI);
		break;
	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
		MOITraceObjectQueryParams.RemoveObjectTypesToQuery(COLLISION_SURFACE_MOI);
		MOITraceObjectQueryParams.RemoveObjectTypesToQuery(COLLISION_DECORATOR_MOI);
		break;
	case EToolMode::VE_SURFACEGRAPH:
		if (CurrentTool->IsInUse())
		{
			MOITraceObjectQueryParams = FCollisionObjectQueryParams(COLLISION_SURFACE_MOI);
		}
		break;
	case EToolMode::VE_FINISH:
	case EToolMode::VE_TRIM:
	case EToolMode::VE_CABINET:
	case EToolMode::VE_PLACEOBJECT:
	case EToolMode::VE_COUNTERTOP:
	default:
		break;
	}

	static const FName MOITraceTag(TEXT("MOITrace"));
	MOITraceQueryParams = FCollisionQueryParams(MOITraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerController), true);
	MOITraceQueryParams.AddIgnoredActors(SnappingActorsToIgnore);
}

bool AEditModelPlayerController::LineTraceSingleAgainstMOIs(struct FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	return GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, MOITraceObjectQueryParams, MOITraceQueryParams);
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
		FModumateCommand(Commands::kSetFOV, true)
		.Param(Parameters::kFieldOfView, FieldOfView));
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

void AEditModelPlayerController::OnHandledInputActionName(FName ActionName, EInputEvent InputEvent)
{
	HandledInputActionEvent.Broadcast(ActionName, InputEvent);
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
		if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("MOUSE MODE: OBJECT")); }

		baseHit = GetObjectMouseHit(mouseLoc, mouseDir, false);

		if (baseHit.Valid)
		{
			const AModumateObjectInstance *hitMOI = Document->ObjectFromActor(baseHit.Actor.Get());
			if (EMPlayerState->ShowDebugSnaps && hitMOI)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("OBJECT HIT #%d, %s"),
					hitMOI->ID, *EnumValueString(EObjectType, hitMOI->GetObjectType())
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

			if (baseHit.Valid && EMPlayerState->ShowDebugSnaps)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("SKETCH HIT")));
			}
		}

		if (!baseHit.Valid && EMPlayerState->ShowDebugSnaps)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("NO OBJECT"));
		}
	}
	// Location based mouse hits check against structure and sketch plane, used by floor and wall tools and handles and other tools that want snap
	else if (EMPlayerState->SnappedCursor.MouseMode == EMouseMode::Location)
	{
		EMPlayerState->SnappedCursor.Visible = true;
		if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("MOUSE MODE: LOCATION")); }

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
			if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("USER POINT DIST %f"), userPointDist)); }
		}

		if (sketchHit.Valid)
		{
			sketchDist = cameraDistance(sketchHit.Location);
			if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("SKETCH DIST %f"), sketchDist)); }
		}

		if (structuralHit.Valid)
		{
			structuralDist = cameraDistance(structuralHit.Location);
			if (EMPlayerState->ShowDebugSnaps)
			{
				if (structuralHit.Actor.IsValid())
				{
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("STRUCTURAL ACTOR %s"), *structuralHit.Actor->GetName()));

					const AModumateObjectInstance *hitMOI = Document->ObjectFromActor(structuralHit.Actor.Get());
					if (hitMOI)
					{
						GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("STRUCTURAL MOI %d, %s"),
							hitMOI->ID, *EnumValueString(EObjectType, hitMOI->GetObjectType())));
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
			if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("STRUCTURAL")); }
			projectedHit = GetShiftConstrainedMouseHit(structuralHit);
			baseHit = structuralHit;
		}
		else if (userPointHit.Valid)
		{
			if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("USER POINT")); }
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

			if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("SKETCH")); }
			projectedHit = GetShiftConstrainedMouseHit(sketchHit);
			baseHit = sketchHit;
		}
		else
		{
			if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("NO HIT")); }
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
			if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("HAVE PROJECTED")); }
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

	if (EMPlayerState->ShowDebugSnaps)
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

	if (InteractionHandle == nullptr && !ShowingModalDialog && actorUnderMouse)
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

bool AEditModelPlayerController::FindBestMousePointHit(const TArray<FVector> &Points,
	const FVector &MouseOrigin, const FVector &MouseDir, float CurObjectHitDist,
	int32 &OutBestIndex, float &OutBestRayDist) const
{
	OutBestIndex = INDEX_NONE;
	OutBestRayDist = FLT_MAX;

	int32 numPoints = Points.Num();
	for (int32 pointIdx = 0; pointIdx < numPoints; ++pointIdx)
	{
		const FVector &point = Points[pointIdx];

		// See if this is our best valid point hit
		float rayDist;
		if (ValidateVirtualHit(MouseOrigin, MouseDir, point,
			CurObjectHitDist, OutBestRayDist, SnapPointMaxScreenDistance, rayDist))
		{
			OutBestRayDist = rayDist;
			OutBestIndex = pointIdx;
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
			FVector affordanceIntercept, mouseIntercept;
			float distance = 0.0f;
			if (UModumateGeometryStatics::FindShortestDistanceBetweenRays(affordance.Origin, customBasis[i], mouseLoc, mouseDir, affordanceIntercept, mouseIntercept, distance))
			{
				if (DistanceBetweenWorldPointsInScreenSpace(affordanceIntercept, mouseIntercept, screenSpaceDist) && (screenSpaceDist < SnapLineMaxScreenDistance))
				{
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
FMouseWorldHitType AEditModelPlayerController::GetObjectMouseHit(const FVector &mouseLoc, const FVector &mouseDir, bool bCheckSnapping) const
{
	// Find the MOI (if any) that we hit
	FMouseWorldHitType objectHit;
	float objectHitDist = FLT_MAX;
	FHitResult hitSingleResult;
	AActor *directHitActor = nullptr;
	FVector directHitNormal(ForceInitToZero);
	if (LineTraceSingleAgainstMOIs(hitSingleResult, mouseLoc, mouseLoc + MaxRaycastDist * mouseDir))
	{
		objectHit.Valid = true;
		objectHit.Actor = hitSingleResult.Actor.Get();
		objectHit.Location = hitSingleResult.Location;
		objectHit.Normal = hitSingleResult.Normal;

		const AModumateObjectInstance* moi = Document->ObjectFromActor(objectHit.Actor.Get());
		EObjectType objectType = moi ? moi->GetObjectType() : EObjectType::OTNone;
		// TODO: this should be an interface function or there should be a way to find the planar object types
		if ((objectType == EObjectType::OTMetaPlane) || (objectType == EObjectType::OTSurfacePolygon))
		{
			FVector moiNormal = moi->GetNormal();
			FPlane plane = FPlane(moi->GetLocation(), moiNormal);
			objectHit.Normal = (moiNormal | objectHit.Normal) > 0 ? moiNormal : -moiNormal;
			objectHit.Location = FVector::PointPlaneProject(objectHit.Location, plane);
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

	// After tracing for a direct hit result against a MOI, now check if it would be overridden by a snap point or line if desired
	if (bCheckSnapping)
	{
		// Update the snapping view (lines and points that can be snapped to)
		int32 mouseQueryBitfield = MOITraceObjectQueryParams.GetQueryBitfield();
		SnappingView->UpdateSnapPoints(SnappingIDsToIgnore, mouseQueryBitfield, true, false);

		CurHitPointLocations.Reset();
		CurHitLineLocations.Reset();
		Algo::Transform(SnappingView->Corners, CurHitPointLocations, [](const FStructurePoint &point) { return point.Point; });
		Algo::Transform(SnappingView->LineSegments, CurHitLineLocations,[](const FStructureLine &line) { return TPair<FVector, FVector>(line.P1, line.P2); });

		if (FindBestMousePointHit(CurHitPointLocations, mouseLoc, mouseDir, objectHitDist, bestVirtualHitIndex, bestVirtualHitDist))
		{
			FStructurePoint &bestPoint = SnappingView->Corners[bestVirtualHitIndex];

			objectHit.Valid = true;
			objectHit.Location = bestPoint.Point;
			objectHit.EdgeDir = bestPoint.Direction;
			objectHit.CP1 = bestPoint.CP1;
			objectHit.CP2 = bestPoint.CP2;

			AModumateObjectInstance *bestObj = Document->GetObjectById(bestPoint.ObjID);
			if (bestObj)
			{
				objectHit.Actor = bestObj->GetActor();
				objectHit.SnapType = (bestPoint.CP2 == INDEX_NONE) ? ESnapType::CT_CORNERSNAP : ESnapType::CT_MIDSNAP;
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
			FStructureLine &bestLine = SnappingView->LineSegments[bestVirtualHitIndex];

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
	}
	// Otherwise, map all of the point- and line-based (virtual) MOIs to locations that can be used to determine a hit result
	else
	{
		CurHitPointMOIs.Reset();
		CurHitPointLocations.Reset();
		CurHitLineMOIs.Reset();
		CurHitLineLocations.Reset();
		static TArray<FStructurePoint> tempPointsForCollision;
		static TArray<FStructureLine> tempLinesForCollision;
		// TODO: we know this is inefficient, should replace with an interface that allows for optimization
		// (like not needing to iterate over every single object in the scene)
		for (AModumateObjectInstance *moi : Document->GetObjectInstances())
		{
			if (moi && moi->IsCollisionEnabled() && moi->UseStructureDataForCollision())
			{
				moi->RouteGetStructuralPointsAndLines(tempPointsForCollision, tempLinesForCollision, false, false);

				// Structural points and lines used for cursor hit collision are mutually exclusive
				if (tempLinesForCollision.Num() > 0)
				{
					for (auto line : tempLinesForCollision)
					{
						CurHitLineMOIs.Add(moi);
						CurHitLineLocations.Add(TPair<FVector, FVector>(line.P1, line.P2));
					}
				}
				else
				{
					for (auto point : tempPointsForCollision)
					{
						CurHitPointMOIs.Add(moi);
						CurHitPointLocations.Add(point.Point);
					}
				}
			}
		}

		// Now find a virtual MOI hit result, preferring points over lines
		if (FindBestMousePointHit(CurHitPointLocations, mouseLoc, mouseDir, objectHitDist, bestVirtualHitIndex, bestVirtualHitDist))
		{
			objectHit.Valid = true;
			objectHit.Actor = CurHitPointMOIs[bestVirtualHitIndex]->GetActor();
			objectHit.Location = CurHitPointLocations[bestVirtualHitIndex];
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

void AEditModelPlayerController::GroupSelected(bool makeGroup)
{
	if (makeGroup)
	{
		TMap<int32, TArray<int32>> parentsToSelectedChildren;
		int32 parentWithMostChildren = 0;
		int32 maxChildCount = 0;

		for (auto *moi : EMPlayerState->SelectedObjects)
		{
			int32 parentID = moi->GetParentID();

			auto &children = parentsToSelectedChildren.FindOrAdd(parentID);
			children.Add(moi->ID);

			int32 numChildren = children.Num();
			if (numChildren > maxChildCount)
			{
				parentWithMostChildren = parentID;
				maxChildCount = numChildren;
			}
		}

		// IDs of selected objects that share a parent
		auto selIds = parentsToSelectedChildren.FindRef(parentWithMostChildren);

		auto *parentObj = Document->GetObjectById(parentWithMostChildren);
		if (parentObj && (parentObj->GetObjectType() == EObjectType::OTGroup) &&
			(maxChildCount == parentObj->GetChildIDs().Num()))
		{
			UE_LOG(LogTemp, Warning, TEXT("Tried to group all of the children of an exist group, ID: %d"),
				parentWithMostChildren);
			return;
		}

		if (maxChildCount > 0)
		{
			Document->BeginUndoRedoMacro();

			auto output = ModumateCommand(
				FModumateCommand(Commands::kGroup)
				.Param(Parameters::kMake, TEXT("true"))
				.Param(Parameters::kObjectIDs, selIds)
				.Param(Parameters::kParent, EMPlayerState->GetViewGroupObjectID())
			);

			int32 groupID = output.GetValue(Parameters::kObjectID);
			if (auto *groupObj = Document->GetObjectById(groupID))
			{
				DeselectAll();
				SetObjectSelected(groupObj, true);
			}

			Document->EndUndoRedoMacro();
		}
	}
	else
	{
		TArray<int32> groupIds;
		Algo::TransformIf(
			EMPlayerState->SelectedObjects,
			groupIds,
			[](const AModumateObjectInstance *ob)
			{
				return ob->GetObjectType() == EObjectType::OTGroup;
			},
			[](const AModumateObjectInstance *ob) 
			{
				return ob->ID; 
			}
		);

		if (groupIds.Num() > 0)
		{
			ModumateCommand(
				FModumateCommand(Commands::kGroup)
				.Param(Parameters::kMake, false)
				.Param(Parameters::kObjectIDs, groupIds)
			);
		}
	}
}

bool AEditModelPlayerController::ToggleGravityPawn()
{
	// Get required pawns
	APlayerCameraManager* camManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	APawn* currentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AEditModelGameMode *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
	if (camManager == nullptr || EMPlayerPawn == nullptr || currentPawn == nullptr || gameMode == nullptr || gameMode->ToggleGravityPawnClass == nullptr)
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
	UModumateAnalyticsStatics::RecordEventSimple(this, UModumateAnalyticsStatics::EventCategoryView, analyticsEventName);

	return true;
}

void AEditModelPlayerController::HandleUndo()
{
	if (EditModelUserWidget->IsBIMDesingerActive())
	{
		return; 
	}

	UModumateDocument* doc = GetDocument();

	if (doc && !doc->IsPreviewingDeltas())
	{
		doc->Undo(GetWorld());
		EMPlayerState->ValidateSelectionsAndView();
	}
}

void AEditModelPlayerController::HandleRedo()
{
	if (EditModelUserWidget->IsBIMDesingerActive())
	{
		return;
	}

	UModumateDocument* doc = GetDocument();

	if (doc && !doc->IsPreviewingDeltas())
	{
		doc->Redo(GetWorld());
		EMPlayerState->ValidateSelectionsAndView();
	}
}
