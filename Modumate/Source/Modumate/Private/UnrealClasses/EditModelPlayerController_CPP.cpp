// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelPlayerController_CPP.h"

#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
#include "BIMKernel/BIMLegacyPortals.h"
#include "Database/ModumateCraftingWidget_CPP.h"
#include "Database/ModumateDrawingSetWidget_CPP.h"
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
#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "UnrealClasses/EditModelCameraController.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelInputAutomation.h"
#include "UnrealClasses/EditModelInputHandler.h"
#include "UnrealClasses/EditModelPlayerPawn_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelToggleGravityPawn_CPP.h"
#include "UnrealClasses/ModumateConsole.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/ModumateObjectInstanceParts_CPP.h"
#include "UnrealClasses/ModumateViewportClient.h"


// Tools
#include "ToolsAndAdjustments/Tools/EditModelCabinetTool.h"
#include "ToolsAndAdjustments/Tools/EditModelCountertopTool.h"
#include "ToolsAndAdjustments/Tools/EditModelCreateSimilarTool.h"
#include "ToolsAndAdjustments/Tools/EditModelCutPlaneTool.h"
#include "ToolsAndAdjustments/Tools/EditModelDrawingTool.h"
#include "ToolsAndAdjustments/Tools/EditModelFFETool.h"
#include "ToolsAndAdjustments/Tools/EditModelFinishTool.h"
#include "ToolsAndAdjustments/Tools/EditModelGraph2DTool.h"
#include "ToolsAndAdjustments/Tools/EditModelJoinTool.h"
#include "ToolsAndAdjustments/Tools/EditModelMetaPlaneTool.h"
#include "ToolsAndAdjustments/Tools/EditModelMoveTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"
#include "ToolsAndAdjustments/Tools/EditModelPortalTools.h"
#include "ToolsAndAdjustments/Tools/EditModelRailTool.h"
#include "ToolsAndAdjustments/Tools/EditModelRoofFaceTool.h"
#include "ToolsAndAdjustments/Tools/EditModelRoofPerimeterTool.h"
#include "ToolsAndAdjustments/Tools/EditModelRotateTool.h"
#include "ToolsAndAdjustments/Tools/EditModelScopeBoxTool.h"
#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"
#include "ToolsAndAdjustments/Tools/EditModelSplitTool.h"
#include "ToolsAndAdjustments/Tools/EditModelStairTool.h"
#include "ToolsAndAdjustments/Tools/EditModelStructureLineTool.h"
#include "ToolsAndAdjustments/Tools/EditModelTrimTool.h"
#include "ToolsAndAdjustments/Tools/EditModelWandTool.h"


static Modumate::PDF::PDFResult PDFLibrary;

using namespace Modumate;

/*
* Constructor
*/
AEditModelPlayerController_CPP::AEditModelPlayerController_CPP()
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
	, HUDDrawWidgetClass(UHUDDrawWidget::StaticClass())
	, CraftingWidgetClass(UModumateCraftingWidget_CPP::StaticClass())
	, DrawingSetWidgetClass(UModumateDrawingSetWidget_CPP::StaticClass())
{
#if !UE_BUILD_SHIPPING
	InputAutomationComponent = CreateDefaultSubobject<UEditModelInputAutomation>(TEXT("InputAutomationComponent"));
#endif

	InputHandlerComponent = CreateDefaultSubobject<UEditModelInputHandler>(TEXT("InputHandlerComponent"));
	CameraController = CreateDefaultSubobject<UEditModelCameraController>(TEXT("CameraController"));
}

AEditModelPlayerController_CPP::~AEditModelPlayerController_CPP()
{
	delete SnappingView;
}

void AEditModelPlayerController_CPP::PostInitializeComponents()
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

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Document = &gameState->Document;

	// Assign our cached casted AEditModelPlayerState_CPP, and its cached casted pointer to us,
	// immediately after it's created.
	AEditModelGameMode_CPP *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
	if ((gameMode != nullptr) && ensureAlways(PlayerState != nullptr))
	{
		EMPlayerState = Cast<AEditModelPlayerState_CPP>(PlayerState);
		EMPlayerState->EMPlayerController = this;
	}
}

/*
* Unreal Event Handlers
*/

void AEditModelPlayerController_CPP::BeginPlay()
{
	Super::BeginPlay();

	EMPlayerPawn = Cast<AEditModelPlayerPawn_CPP>(GetPawn());
	if (ensure(EMPlayerPawn))
	{
		SetViewTargetWithBlend(EMPlayerPawn);
	}

	SnappingView = new Modumate::FModumateSnappingView(Document);

	AEditModelGameMode_CPP *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();

	CreateTools();
	SetToolMode(EToolMode::VE_SELECT);

	CraftingWidget = CreateWidget<UModumateCraftingWidget_CPP>(this, CraftingWidgetClass);
	if (CraftingWidget != nullptr)
	{
		CraftingWidget->AddToViewport();
		CraftingWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	DrawingSetWidget = CreateWidget<UModumateDrawingSetWidget_CPP>(this, DrawingSetWidgetClass);
	if (DrawingSetWidget != nullptr)
	{
		DrawingSetWidget->AddToViewport();
		DrawingSetWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	HUDDrawWidget = CreateWidget<UHUDDrawWidget>(this, HUDDrawWidgetClass);
	if (HUDDrawWidget != nullptr)
	{
		HUDDrawWidget->AddToViewport();
		HUDDrawWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// If we have a crash recovery, load that 
	// otherwise if the game mode started with a pending project (if it came from the command line, for example) then load it immediately. 

	FString fileLoadPath;
	bool addToRecents = true;

	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	FString recoveryFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateRecoveryFile);
		
	if (gameInstance->RecoveringFromCrash)
	{
		fileLoadPath = recoveryFile;
		addToRecents = false;
		gameInstance->RecoveringFromCrash = false;
	}
	else
	{
		fileLoadPath = gameMode->PendingProjectPath;
		addToRecents = true;
	}

	if (!fileLoadPath.IsEmpty())
	{
		LoadModelFilePath(fileLoadPath, addToRecents);
	}
	else
	{
		Document->MakeNew(GetWorld());
	}

	EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	FString lockFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);
	FFileHelper::SaveStringToFile(TEXT("lock"), *lockFile);

	GetWorldTimerManager().SetTimer(ControllerTimer, this, &AEditModelPlayerController_CPP::OnControllerTimer, 10.0f, true, 0.0f);

	TimeOfLastAutoSave = FDateTime::Now();

#if !UE_BUILD_SHIPPING
	// Now that we've registered our commands, register them in the UE4 console's autocompletion list
	UModumateConsole *console = Cast<UModumateConsole>(GEngine->GameViewport ? GEngine->GameViewport->ViewportConsole : nullptr);
	if (console)
	{
		console->BuildRuntimeAutoCompleteList(true);
	}
#endif
}

void AEditModelPlayerController_CPP::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	FString lockFile = FPaths::Combine(gameInstance->UserSettings.GetLocalTempDir(), kModumateCleanShutdownFile);
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*lockFile);
	GetWorldTimerManager().ClearTimer(ControllerTimer);
	Super::EndPlay(EndPlayReason);
}

bool AEditModelPlayerController_CPP::ToolIsInUse() const
{
	return (CurrentTool && CurrentTool->IsInUse());
}

EToolMode AEditModelPlayerController_CPP::GetToolMode()
{
	return CurrentTool ? CurrentTool->GetToolMode() : EToolMode::VE_NONE;
}

void AEditModelPlayerController_CPP::SetToolMode(EToolMode NewToolMode)
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
		// TODO: runtime assemblies to be replaced with presets 
		// meantime this will ensure tools start with a default assembly
		if (CurrentTool->GetAssembly().Key.IsNone())
		{
			FModumateObjectAssembly assembly;
			AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
			if (gameState->Document.PresetManager.TryGetDefaultAssemblyForToolMode(NewToolMode, assembly))
			{
				CurrentTool->SetAssembly(assembly.AsShoppingItem());
			}
		}
		CurrentTool->Activate();
	}

	UpdateMouseTraceParams();
	EMPlayerState->UpdateObjectVisibilityAndCollision();
}

void AEditModelPlayerController_CPP::AbortUseTool()
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

void AEditModelPlayerController_CPP::SetToolAxisConstraint(EAxisConstraint AxisConstraint)
{
	if (CurrentTool)
	{
		CurrentTool->SetAxisConstraint(AxisConstraint);
	}
}

void AEditModelPlayerController_CPP::SetToolCreateObjectMode(EToolCreateObjectMode CreateObjectMode)
{
	if (CurrentTool)
	{
		CurrentTool->SetCreateObjectMode(CreateObjectMode);
	}
}

bool AEditModelPlayerController_CPP::HandleToolInputText(FString InputText)
{
	EToolMode curToolMode = GetToolMode();
	float v = 0.f;
	// Most cases input is in imperial unit, unless is specific handle or tool mode
	if (curToolMode == EToolMode::VE_ROTATE || // Rotate tool uses degree
		(curToolMode == EToolMode::VE_FLOOR && EMPlayerState->CurrentDimensionStringGroupIndex == 1) || // Floor tool degree string uses degree
		(curToolMode == EToolMode::VE_COUNTERTOP && EMPlayerState->CurrentDimensionStringGroupIndex == 1) || // CounterTop tool degree string uses degree
		CanCurrentHandleShowRawInput())
	{
		v = FCString::Atof(*InputText);
	}
	else
	{
		v = UModumateDimensionStatics::StringToMetric(InputText, true);
	}

	const float MAX_DIMENSION = 525600 * 12 * 2.54;

	if (v != 0.0f && v <= MAX_DIMENSION)
	{
		// First, try to use the player controller's input number handling,
		// in case a user snap point is taking input.
		if (HandleInputNumber(v))
		{
			return true;
		}

		if (InteractionHandle)
		{
			if (InteractionHandle->HandleInputNumber(v))
			{
				InteractionHandle = nullptr;
				return true;
			}
		}
		else if (CurrentTool && CurrentTool->IsInUse())
		{
			return CurrentTool->HandleInputNumber(v);
		}
	}

	return false;
}

bool AEditModelPlayerController_CPP::CanCurrentHandleShowRawInput()
{
	if (InteractionHandle)
	{
		return InteractionHandle->AcceptRawInputNumber;
	}
	return false;
}

void AEditModelPlayerController_CPP::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputHandlerComponent->SetupBindings();

	if (InputComponent)
	{
		// Bind raw inputs, so that we have better control over input logging and forwarding than we do in Blueprint.
		InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Pressed, this, &AEditModelPlayerController_CPP::HandleRawLeftMouseButtonPressed);
		InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Released, this, &AEditModelPlayerController_CPP::HandleRawLeftMouseButtonReleased);

		// Add an input to test a crash.
		InputComponent->BindKey(FInputChord(EKeys::Backslash, true, true, true, false), EInputEvent::IE_Pressed, this, &AEditModelPlayerController_CPP::DebugCrash);

#if !UE_BUILD_SHIPPING
		// Add a debug command to force clean selected objects (Ctrl+Shift+R)
		InputComponent->BindKey(FInputChord(EKeys::R, true, true, false, false), EInputEvent::IE_Pressed, this, &AEditModelPlayerController_CPP::CleanSelectedObjects);
#endif
	}
}

void AEditModelPlayerController_CPP::CreateTools()
{
	RegisterTool(CreateTool<USelectTool>());
	RegisterTool(CreateTool<UPlaceObjectTool>());
	RegisterTool(CreateTool<UMoveObjectTool>());
	RegisterTool(CreateTool<URotateObjectTool>());
	RegisterTool(CreateTool<USplitObjectTool>());
	RegisterTool(CreateTool<UWallTool>());
	RegisterTool(CreateTool<UFloorTool>());
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
	RegisterTool(CreateTool<UMetaPlaneTool>());
	RegisterTool(CreateTool<UCutPlaneTool>());
	RegisterTool(CreateTool<UScopeBoxTool>());
	RegisterTool(CreateTool<UJoinTool>());
	RegisterTool(CreateTool<UCreateSimilarTool>());
	RegisterTool(CreateTool<UStructureLineTool>());
	RegisterTool(CreateTool<UDrawingTool>());
	RegisterTool(CreateTool<UGraph2DTool>());
}

void AEditModelPlayerController_CPP::RegisterTool(TScriptInterface<IEditModelToolInterface> NewTool)
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

	ModeToTool.Add(newToolMode, NewTool);
}

void AEditModelPlayerController_CPP::OnLButtonDown()
{
	// TODO: for now, using tools may create dimension strings that are incompatible with those created
	// by the current user snap point, so we'll clear them now. In the future, we want to keep track of
	// all of these dimension strings consistently so that we can cycle between them.
	ClearUserSnapPoints();

	if (InteractionHandle)
	{
		InteractionHandle->EndUse();
		InteractionHandle = nullptr;
		return;
	}

	if (HoverHandle)
	{
		if (HoverHandle->BeginUse())
		{
			InteractionHandle = HoverHandle;
		}
		// If the handle reported failure beginning, but it is in use, then abort it.
		else if (!ensureAlways(!HoverHandle->IsInUse()))
		{
			HoverHandle->AbortUse();
		}
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

void AEditModelPlayerController_CPP::OnLButtonUp()
{
	if (CurrentTool && CurrentTool->IsInUse())
	{
		CurrentTool->HandleMouseUp();
	}
}

/*
Load/Save menu
*/

bool AEditModelPlayerController_CPP::SaveModel()
{
	if (EMPlayerState->LastFilePath.IsEmpty())
	{
		return SaveModelAs();
	}
	return SaveModelFilePath(EMPlayerState->LastFilePath);
}

bool AEditModelPlayerController_CPP::SaveModelAs()
{
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

bool AEditModelPlayerController_CPP::SaveModelFilePath(const FString &filepath)
{
	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	// Try to capture a thumbnail for the project	
	CaptureProjectThumbnail();
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	if (gameState->Document.Save(GetWorld(), filepath))
	{
		EMPlayerState->LastFilePath = filepath;
		Modumate::PlatformFunctions::ShowMessageBox(*FString::Printf(TEXT("Saved as %s"),*EMPlayerState->LastFilePath), TEXT("Save Model"), Modumate::PlatformFunctions::Okay);

		static const FString eventCategory(TEXT("FileIO"));
		static const FString eventName(TEXT("SaveDocument"));
		UModumateAnalyticsStatics::RecordEventSimple(this, eventCategory, eventName);
		return true;
	}
	else
	{
		Modumate::PlatformFunctions::ShowMessageBox(*FString::Printf(TEXT("COULD NOT SAVE %s"), *EMPlayerState->LastFilePath), TEXT("Save Model"), Modumate::PlatformFunctions::Okay);
		return false;
	}
}

bool AEditModelPlayerController_CPP::LoadModel()
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

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();

	FString filename;
	bool bLoadSuccess = false;

	if (Modumate::PlatformFunctions::GetOpenFilename(filename))
	{
		bLoadSuccess = gameState->Document.Load(GetWorld(), filename, true);
		if (bLoadSuccess)
		{
			EMPlayerState->LastFilePath = filename;
		}
		else
		{
			EMPlayerState->LastFilePath.Empty();
		}
	}

	EMPlayerState->ShowingFileDialog = false;
	return bLoadSuccess;
}

bool AEditModelPlayerController_CPP::LoadModelFilePath(const FString &filename,bool addToRecents)
{
	EMPlayerState->OnNewModel();
	if (addToRecents)
	{
		EMPlayerState->LastFilePath = filename;
	}
	else
	{
		EMPlayerState->LastFilePath.Empty();
	}
	return Document->Load(GetWorld(),filename, addToRecents);
}

bool AEditModelPlayerController_CPP::CheckSaveModel()
{
	if (Document->IsDirty())
	{
		Modumate::PlatformFunctions::EMessageBoxResponse resp = Modumate::PlatformFunctions::ShowMessageBox(TEXT("Save current model?"), TEXT("New Model"), Modumate::PlatformFunctions::YesNoCancel);
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

void AEditModelPlayerController_CPP::NewModel()
{
	if (!EMPlayerState->ShowingFileDialog)
	{
		if (!CheckSaveModel())
		{
			return;
		}

		EMPlayerState->OnNewModel();
		ModumateCommand(FModumateCommand(Commands::kMakeNew));
	}
}

void AEditModelPlayerController_CPP::SetThumbnailCapturer(ASceneCapture2D* InThumbnailCapturer)
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

bool AEditModelPlayerController_CPP::CaptureProjectThumbnail()
{
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	USceneCaptureComponent2D *captureComp = ThumbnailCapturer ? ThumbnailCapturer->GetCaptureComponent2D() : nullptr;
	if (gameState && captureComp && captureComp->TextureTarget)
	{
		// First, position the thumbnail capturer in a position that can see the whole project
		float captureAspect = (float)captureComp->TextureTarget->SizeX / (float)captureComp->TextureTarget->SizeY;
		float captureMaxAspect = FMath::Max(captureAspect, 1.0f / captureAspect);

		FSphere projectBounds = gameState->Document.CalculateProjectBounds().GetSphere();
		FVector captureOrigin = CalculateViewLocationForSphere(projectBounds, captureComp->GetForwardVector(), captureMaxAspect, captureComp->FOVAngle);
		captureComp->GetOwner()->SetActorLocation(captureOrigin);

		// Perform the actual scene capture
		captureComp->CaptureScene();

		// Now, read the render texture contents, compress it, and encode it as a string for the project metadata.
		FString encodedThumbnail;
		if (gameState && FModumateThumbnailHelpers::CreateProjectThumbnail(captureComp->TextureTarget, encodedThumbnail))
		{
			gameState->Document.CurrentEncodedThumbnail = encodedThumbnail;
			return true;
		}
	}

	return false;
}

bool AEditModelPlayerController_CPP::GetScreenshotFileNameWithDialog(FString &filename)
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

void AEditModelPlayerController_CPP::SetCutPlaneVisibility(bool bVisible)
{
	bCutPlaneVisible = bVisible;
	EMPlayerState->UpdateObjectVisibilityAndCollision();
}

void AEditModelPlayerController_CPP::TrySavePDF()
{
	OnSavePDF();
}

bool AEditModelPlayerController_CPP::OnSavePDF()
{
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

bool AEditModelPlayerController_CPP::OnCreateDwg()
{
	EMPlayerState->ShowingFileDialog = true;

	if (ToolIsInUse())
	{
		AbortUseTool();
	}

	bool retValue = true;

	UModumateGameInstance* gameInstance = dynamic_cast<UModumateGameInstance*>(GetGameInstance());
	if (!gameInstance)
	{
		return false;
	}

	static const FText dialogTitle = FText::FromString(FString(TEXT("DWG Creation")));
	if (gameInstance->LoginStatus() != ELoginStatus::Connected)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString(FString(TEXT("You must be logged in to use the DWG server")) ),
			&dialogTitle);
		return false;
	}

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

void AEditModelPlayerController_CPP::DeleteActionDefault()
{
	ModumateCommand(
		FModumateCommand(Commands::kDeleteSelectedObjects)
		.Param(Parameters::kIncludeConnected, true)
	);
}

void AEditModelPlayerController_CPP::DeleteActionOnlySelected()
{
	ModumateCommand(
		FModumateCommand(Commands::kDeleteSelectedObjects)
		.Param(Parameters::kIncludeConnected, false)
	);
}

bool AEditModelPlayerController_CPP::HandleTabKeyForDimensionString()
{
	// Loop through dimension strings from the same groupID
	// TODO: Use dim string uniqueID and the currentGroupID from playerState to identify which dim strings to loop through
	EMPlayerState->CurrentDimensionStringGroupIndex = (EMPlayerState->CurrentDimensionStringGroupIndex + 1) % (EMPlayerState->CurrentDimensionStringGroupIndexMax + 1);

	// Check if there's any EditableTextBox widget
	// Currently portal objects have 2 widget for controlling its width and height
	// Hitting Tab allows user to switch between vertical and horizontal text box
	if (DimStringWidgetSelectedObject)
	{
		if (EnablePortalVerticalInput)
		{
			EnablePortalVerticalInput = false; EnablePortalHorizontalInput = true;
		}
		else
		{
			EnablePortalHorizontalInput = false; EnablePortalVerticalInput = true;
		}
		return true;
	}

	// Allow switch between drawing delta and total dimension string based on current handle object type
	if (!InteractionHandle)
	{
		return false;
	}
	if (!InteractionHandle->ParentMOI)
	{
		return false;
	}
	if (EnableDrawTotalLine)
	{
		EnableDrawTotalLine = false; EnableDrawDeltaLine = true;
	}
	else
	{
		EnableDrawTotalLine = true; EnableDrawDeltaLine = false;
	}
	return true;
}

void AEditModelPlayerController_CPP::ResetDimensionStringTabState()
{
	EnableDrawDeltaLine = true;
	EnableDrawTotalLine = false;
	EMPlayerState->CurrentDimensionStringGroupIndex = 0;
}

bool AEditModelPlayerController_CPP::HandleInputNumber(double inputNumber)
{
	// This input number handler is intended as a fallback, when the player state's tools do not handle it.
	// TODO: reconcile the back-and-forth input handling between PlayerController and PlayerState.

	// If there's an active tool or handle that can receive the user snap point input, then use it here.

	AAdjustmentHandleActor_CPP *curHandle = InteractionHandle;

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
				return curHandle->EndUse();
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

	// Check if there's text box widget that can accept user input
	if (DimStringWidgetSelectedObject)
	{
		bool bCanSetPortalDimension = false;
		FModumateObjectInstance *moi = Document->ObjectFromActor(DimStringWidgetSelectedObject);
		if (moi != nullptr)
		{
			if ((moi->GetObjectType() == EObjectType::OTDoor) ||
				(moi->GetObjectType() == EObjectType::OTWindow))
			{
				bCanSetPortalDimension = true;
			}
		}
		if (bCanSetPortalDimension)
		{
			float userInput = UModumateDimensionStatics::StringToMetric(GetTextBoxUserInput());
			if (EnablePortalVerticalInput)
			{
				UModumateFunctionLibrary::MoiPortalSetNewHeight(DimStringWidgetSelectedObject, userInput);
			}
			if (EnablePortalHorizontalInput)
			{
				UModumateFunctionLibrary::MoiPortalSetNewWidth(DimStringWidgetSelectedObject, userInput);
			}
			return true;
		}
	}

	return false;
}

bool AEditModelPlayerController_CPP::HandleInvert()
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

bool AEditModelPlayerController_CPP::HandleEscapeKey()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::HandleEscapeKey"));

	ClearUserSnapPoints();
	EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	if (HoverHandle != nullptr)
	{
		HoverHandle = nullptr;
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
	else if (GetPawn<AEditModelToggleGravityPawn_CPP>())
	{
		ToggleGravityPawn();
	}
	return false;
}

bool AEditModelPlayerController_CPP::IsShiftDown() const
{
	return IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
}

bool AEditModelPlayerController_CPP::IsControlDown() const
{
	return IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
}


void AEditModelPlayerController_CPP::OnControllerTimer()
{
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());

	if (gameInstance->UserSettings.AutoBackup)
	{
		FTimespan ts(FDateTime::Now().GetTicks() - TimeOfLastAutoSave.GetTicks());
		if (ts.GetTotalSeconds() > gameInstance->UserSettings.AutoBackupFrequencySeconds)
		{
			WantAutoSave = true;
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("Edit tick"), STAT_ModumateEditTick, STATGROUP_Modumate)

void AEditModelPlayerController_CPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SCOPE_CYCLE_COUNTER(STAT_ModumateEditTick);

	if (WantAutoSave)
	{
		if (InteractionHandle == nullptr && !ToolIsInUse())
		{
			UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());

			FString tempDir = gameInstance->UserSettings.GetLocalTempDir();
			FString oldFile = FPaths::Combine(tempDir, kModumateRecoveryFileBackup);
			FString newFile = FPaths::Combine(tempDir, kModumateRecoveryFile);

			//Look for the first available (of 3) backup files and delete the one following it
			FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*oldFile);
			FPlatformFileManager::Get().GetPlatformFile().MoveFile(*oldFile, *newFile);
			AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
			gameState->Document.Save(GetWorld(), newFile);

			TimeOfLastAutoSave = FDateTime::Now();
			WantAutoSave = false;
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

		AEditModelGameMode_CPP *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
		TArray<FString> dbstrs = gameMode->ObjectDatabase->GetDebugInfo();
		for (auto &str : dbstrs)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, str, false);
		}

		for (auto &sob : EMPlayerState->SelectedObjects)
		{
			FVector p = sob->GetObjectLocation();
			GetWorld()->LineBatcher->DrawLine(p - FVector(20, 0, 0), p + FVector(20, 0, 0), FColor::Blue, SDPG_MAX, 2, 0.0);
			GetWorld()->LineBatcher->DrawLine(p - FVector(0, 20, 0), p + FVector(0, 20, 0), FColor::Blue, SDPG_MAX, 2, 0.0);

			FVector d = sob->GetObjectRotation().RotateVector(FVector(40, 0, 0));
			GetWorld()->LineBatcher->DrawLine(p, p + d, FColor::Green, SDPG_MAX, 2, 0.0);

			for (auto &cp : sob->GetControlPoints())
			{
				GetWorld()->LineBatcher->DrawLine(cp - FVector(20, 0, 0), cp + FVector(20, 0, 0), FColor::Blue, SDPG_MAX, 2, 0.0);
				GetWorld()->LineBatcher->DrawLine(cp - FVector(0, 20, 0), cp + FVector(0, 20, 0), FColor::Blue, SDPG_MAX, 2, 0.0);
			}
		}
	}

	if (EMPlayerState->bShowVolumeDebug)
	{
		Document->DrawDebugVolumeGraph(GetWorld());
	}
}

void AEditModelPlayerController_CPP::TickInput(float DeltaTime)
{
	bIsCursorAtAnyWidget = IsCursorOverWidget();
	FSnappedCursor &cursor = EMPlayerState->SnappedCursor;

	if (!CameraInputLock && (CameraController->GetMovementState() == ECameraMovementState::Default))
	{
		GetMousePosition(cursor.ScreenPosition.X, cursor.ScreenPosition.Y);

		UpdateMouseHits(DeltaTime);
		UpdateUserSnapPoint();

		if (InteractionHandle != nullptr)
		{
			if (!InteractionHandle->UpdateUse())
			{
				InteractionHandle->EndUse();
				InteractionHandle = nullptr;
			}
			UpdateAffordances();
		}
		else if (CurrentTool != nullptr)
		{
			CurrentTool->FrameUpdate();
			if (CurrentTool->IsInUse() && CurrentTool->ShowSnapCursorAffordances())
			{
				UpdateAffordances();
			}
		}
	}
	else
	{
		EMPlayerState->SnappedCursor.Visible = false;
	}

	Document->CleanObjects();
}

FVector AEditModelPlayerController_CPP::CalculateViewLocationForSphere(const FSphere &TargetSphere, const FVector &ViewVector, float AspectRatio, float FOV)
{
	float captureHalfFOV = 0.5f * FMath::DegreesToRadians(FOV);
	float captureDistance = AspectRatio * TargetSphere.W / FMath::Tan(captureHalfFOV);
	return TargetSphere.Center - captureDistance * ViewVector;
}

bool AEditModelPlayerController_CPP::ZoomToProjectExtents()
{
	// Only perform zoom to extent when controller is using EMPlayerPawn
	if (UGameplayStatics::GetPlayerPawn(this, 0) != EMPlayerPawn)
	{
		return false;
	}

	// Calculate project bound, perform no action if it's too small
	FSphere projectBounds = Document->CalculateProjectBounds().GetSphere();
	if (projectBounds.W <= 1.f)
	{
		return false;
	}
	FVector captureOrigin = CalculateViewLocationForSphere(
			projectBounds, 
			EMPlayerPawn->CameraComponent->GetForwardVector(), 
			EMPlayerPawn->CameraComponent->AspectRatio,
			EMPlayerPawn->CameraComponent->FieldOfView);

	EMPlayerPawn->CameraComponent->SetWorldLocation(captureOrigin);
	return true;
}

bool AEditModelPlayerController_CPP::ZoomToSelection()
{
	// Only perform zoom to extent when controller is using EMPlayerPawn
	if (UGameplayStatics::GetPlayerPawn(this, 0) != EMPlayerPawn)
	{
		return false;
	}
	// Calculate selection bound, perform no action if it's too small
	FBoxSphereBounds selectedBound = UModumateFunctionLibrary::GetSelectedExtents(this);
	if (selectedBound.SphereRadius <= 1.f)
	{
		return false;
	}

	FVector captureOrigin = CalculateViewLocationForSphere(
		selectedBound.GetSphere(),
		EMPlayerPawn->CameraComponent->GetForwardVector(),
		EMPlayerPawn->CameraComponent->AspectRatio,
		EMPlayerPawn->CameraComponent->FieldOfView);

	EMPlayerPawn->CameraComponent->SetWorldLocation(captureOrigin);
	return true;
}

////

/*
* Mouse Functions
*/

void AEditModelPlayerController_CPP::SetObjectSelected(const FModumateObjectInstance *ob, bool selected)
{
	ModumateCommand(
		FModumateCommand(Commands::kSelectObject)
		.Param(Parameters::kObjectID, ob->ID)
		.Param(Parameters::kSelected, selected)
	);
}

bool AEditModelPlayerController_CPP::InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	// First, pass through the input
	bool bResult = Super::InputKey(Key, EventType, AmountDepressed, bGamepad);

	// Check to see if we're recording input (and not playing it back); if so, then log it
	if (InputAutomationComponent && InputAutomationComponent->IsRecording() &&
		!bGamepad && !InputAutomationComponent->IsPlaying())
	{
		InputAutomationComponent->RecordInput(Key, EventType);
	}

	return bResult;
}

void AEditModelPlayerController_CPP::HandleRawLeftMouseButtonPressed()
{
	HandleLeftMouseButton(true);
}

void AEditModelPlayerController_CPP::HandleRawLeftMouseButtonReleased()
{
	HandleLeftMouseButton(false);
}

void AEditModelPlayerController_CPP::HandleLeftMouseButton_Implementation(bool bPressed)
{
	// TODO: handle input logic here, see OnLButtonDown, OnLButtonUp
}

void AEditModelPlayerController_CPP::SelectAll()
{
	ModumateCommand(FModumateCommand(Commands::kSelectAll));
}

void AEditModelPlayerController_CPP::SelectInverse()
{
	ModumateCommand(FModumateCommand(Commands::kSelectInverse));
}

void AEditModelPlayerController_CPP::DeselectAll()
{
	ModumateCommand(FModumateCommand(Commands::kDeselectAll));
}

bool AEditModelPlayerController_CPP::SelectObjectById(int32 ObjectID)
{
	const FModumateObjectInstance *moi = Document->GetObjectById(ObjectID);
	if (moi == nullptr)
	{
		return false;
	}

	DeselectAll();
	SetObjectSelected(moi, true);

	return true;
}

FTransform AEditModelPlayerController_CPP::MakeUserSnapPointFromCursor(const FSnappedCursor &cursor) const
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

bool AEditModelPlayerController_CPP::CanMakeUserSnapPointAtCursor(const FSnappedCursor &cursor) const
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

void AEditModelPlayerController_CPP::ToggleUserSnapPoint()
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

void AEditModelPlayerController_CPP::AddUserSnapPoint(const FTransform &snapPoint)
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

void AEditModelPlayerController_CPP::ClearUserSnapPoints()
{
	UserSnapPoints.Reset();
}

namespace { volatile char* ptr; }
void AEditModelPlayerController_CPP::DebugCrash()
{
	UE_LOG(LogTemp, Error, TEXT("DebugCrash"));

	if (bAllowDebugCrash)
	{
		memset((void*)ptr, 0x42, 1024 * 1024 * 20);
	}
}

void AEditModelPlayerController_CPP::CleanSelectedObjects()
{
	if (!ensure(EMPlayerState && Document))
	{
		return;
	}

	TArray<FModumateObjectInstance *> objectsToClean;
	if (EMPlayerState->SelectedObjects.Num() == 0)
	{
		objectsToClean.Append(Document->GetObjectInstances());
	}
	else
	{
		objectsToClean.Append(EMPlayerState->SelectedObjects);
	}

	for (auto *obj : objectsToClean)
	{
		for (EObjectDirtyFlags dirtyFlag : UModumateTypeStatics::OrderedDirtyFlags)
		{
			obj->MarkDirty(dirtyFlag);
		}
	}

	Document->CleanObjects();
}

void AEditModelPlayerController_CPP::SetViewGroupObject(const FModumateObjectInstance *ob)
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
void AEditModelPlayerController_CPP::UpdateDefaultWallHeight(float newHeight)
{
	if (ensureAlways(Document != nullptr))
	{
		Document->SetDefaultWallHeight(newHeight);
	}
}

float AEditModelPlayerController_CPP::GetDefaultWallHeightFromDoc() const
{
	if (ensureAlways(Document != nullptr))
	{
		return Document->GetDefaultWallHeight();
	}
	return 0.f;
}

void AEditModelPlayerController_CPP::UpdateDefaultWindowHeightWidth(float newHeight, float newWidth)
{
	Document->SetDefaultWindowHeightWidth(newHeight, newWidth);
}

float AEditModelPlayerController_CPP::GetDefaultWindowHeightFromDoc() const
{
	return Document->GetDefaultWindowHeight();
}

void AEditModelPlayerController_CPP::UpdateDefaultJustificationZ(float newValue)
{
	Document->SetDefaultJustificationZ(newValue);
}

void AEditModelPlayerController_CPP::UpdateDefaultJustificationXY(float newValue)
{
	Document->SetDefaultJustificationXY(newValue);
}

float AEditModelPlayerController_CPP::GetDefaultJustificationZ() const
{
	return Document->GetDefaultJustificationZ();
}

float AEditModelPlayerController_CPP::GetDefaultJustificationXY() const
{
	return Document->GetDefaultJustificationXY();
}

bool AEditModelPlayerController_CPP::GetMetaPlaneHostedObjJustificationValue(AActor* actor, float& value)
{
	if (actor != nullptr)
	{
		FModumateObjectInstance *moi = Document->ObjectFromActor(actor);
		FModumateObjectInstance *parentObj = moi ? moi->GetParentObject() : nullptr;
		if (parentObj && (parentObj->GetObjectType() == EObjectType::OTMetaPlane))
		{
			value = moi->GetExtents().X;
			return true;
		}
	}

	return false;
}

void AEditModelPlayerController_CPP::SetMetaPlaneHostedObjJustificationValue(float newValue, AActor* actor)
{
	if (actor != nullptr)
	{
		FModumateObjectInstance *moi = Document->ObjectFromActor(actor);
		FModumateObjectInstance *parentObj = moi ? moi->GetParentObject() : nullptr;
		if (parentObj && (parentObj->GetObjectType() == EObjectType::OTMetaPlane))
		{
			TArray<FVector> points;
			auto result = ModumateCommand(
				FModumateCommand(Commands::kSetGeometry)
				.Param(Parameters::kObjectID, moi->ID)
				.Param(Parameters::kControlPoints, points)
				.Param(Parameters::kExtents, FVector(newValue, 0.0f, 0.0f))
			);
		}
	}
}

void AEditModelPlayerController_CPP::ToolAbortUse()
{
	AbortUseTool();
}

void AEditModelPlayerController_CPP::IgnoreObjectIDForSnapping(int32 ObjectID)
{
	SnappingIDsToIgnore.Add(ObjectID);
	UpdateMouseTraceParams();
}

void AEditModelPlayerController_CPP::ClearIgnoredIDsForSnapping()
{
	SnappingIDsToIgnore.Reset();
	UpdateMouseTraceParams();
}

void AEditModelPlayerController_CPP::UpdateMouseTraceParams()
{
	SnappingActorsToIgnore.Reset();
	SnappingActorsToIgnore.Add(this);

	const TArray<FModumateObjectInstance *> objs = Document->GetObjectInstances();
	for (FModumateObjectInstance *obj : objs)
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
	case EToolMode::VE_ROOF_FACE:
	case EToolMode::VE_RAIL:
	case EToolMode::VE_STAIR:
	case EToolMode::VE_METAPLANE:
		MOITraceObjectQueryParams = FCollisionObjectQueryParams(COLLISION_META_MOI);
		break;
	case EToolMode::VE_FINISH:
	case EToolMode::VE_TRIM:
	case EToolMode::VE_PLACEOBJECT:
	case EToolMode::VE_DOOR:
	case EToolMode::VE_WINDOW:
	case EToolMode::VE_CABINET:
	case EToolMode::VE_COUNTERTOP:
		MOITraceObjectQueryParams.RemoveObjectTypesToQuery(COLLISION_META_MOI);
		MOITraceObjectQueryParams.RemoveObjectTypesToQuery(COLLISION_DECORATOR_MOI);
		break;
	default:
		break;
	}

	static const FName MOITraceTag(TEXT("MOITrace"));
	MOITraceQueryParams = FCollisionQueryParams(MOITraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerController), true);
	MOITraceQueryParams.AddIgnoredActors(SnappingActorsToIgnore);
}

bool AEditModelPlayerController_CPP::LineTraceSingleAgainstMOIs(struct FHitResult& OutHit, const FVector& Start, const FVector& End) const
{
	return GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, MOITraceObjectQueryParams, MOITraceQueryParams);
}

bool AEditModelPlayerController_CPP::IsCursorOverWidget() const
{
	FSlateApplication &slateApp = FSlateApplication::Get();
	TSharedPtr<FSlateUser> slateUser = slateApp.GetUser(0);
	TSharedPtr<SViewport> gameViewport = slateApp.GetGameViewport();
	FWeakWidgetPath widgetPath = slateUser->GetLastWidgetsUnderCursor();
	TWeakPtr<SWidget> hoverWidgetWeak = widgetPath.IsValid() ? widgetPath.GetLastWidget() : TWeakPtr<SWidget>();

	if (!hoverWidgetWeak.IsValid())
	{
		return false;
	}

	TSharedPtr<SWidget> hoverWidget = hoverWidgetWeak.Pin();
	if ((hoverWidget == gameViewport) || hoverWidget->Advanced_IsWindow())
	{
		return false;
	}

	return true;
}

bool AEditModelPlayerController_CPP::IsCraftingWidgetActive() const
{
	return CraftingWidget && CraftingWidget->IsVisible();
}

void AEditModelPlayerController_CPP::SetFieldOfViewCommand(float FieldOfView)
{
	ModumateCommand(
		FModumateCommand(Commands::kSetFOV, true)
		.Param(Parameters::kFieldOfView, FieldOfView));
}

bool AEditModelPlayerController_CPP::GetActiveUserSnapPoint(FTransform &outSnapPoint) const
{
	if (EMPlayerState->SnappedCursor.Visible && (UserSnapPoints.Num() > 0) &&
		(EMPlayerState->SnappedCursor.MouseMode == EMouseMode::Location))
	{
		outSnapPoint = UserSnapPoints.Last();
		return true;
	}

	return false;
}

float AEditModelPlayerController_CPP::DistanceBetweenWorldPointsInScreenSpace(const FVector &p1, const FVector &p2) const
{
	FVector2D sp1, sp2;
	ProjectWorldLocationToScreen(p1, sp1);
	ProjectWorldLocationToScreen(p2, sp2);

	return (sp1 - sp2).Size();
}

/*
Called every frame, based on mouse mode set in the player state,
we put the mouse's snapped position and projected position into
the SnappedCursor structure, which is read by EditModelPlayerHUD_BP
*/
void AEditModelPlayerController_CPP::UpdateMouseHits(float deltaTime)
{
	FVector mouseLoc, mouseDir;
	FMouseWorldHitType baseHit, projectedHit;

	// Reset frame defaults
	baseHit.Valid = false;
	projectedHit.Valid = false;
	baseHit.Actor = nullptr;
	projectedHit.Actor = nullptr;

	EMPlayerState->SnappedCursor.bValid = false;
	EMPlayerState->SnappedCursor.Visible = false;
	EMPlayerState->SnappedCursor.SnapType = ESnapType::CT_NOSNAP;

	AActor *actorUnderMouse = nullptr;

	// If we're off the screen or hovering at widgets, then return with the snapped cursor turned off
	if (!DeprojectMousePositionToWorld(mouseLoc, mouseDir) || bIsCursorAtAnyWidget)
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

		baseHit = UpdateHandleHit(mouseLoc, mouseDir);
		if (baseHit.Valid)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("HANDLE HIT: %s"), *baseHit.Actor->GetName()));
		}
		else
		{
			baseHit = GetObjectMouseHit(mouseLoc, mouseDir, false);

			if (baseHit.Valid)
			{
				const FModumateObjectInstance *hitMOI = Document->ObjectFromActor(baseHit.Actor.Get());
				if (EMPlayerState->ShowDebugSnaps && hitMOI)
				{
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, FString::Printf(TEXT("OBJECT HIT #%d, %s"),
						hitMOI->ID, *EnumValueString(EObjectType, hitMOI->GetObjectType())
					));
				}
				projectedHit = GetShiftConstrainedMouseHit(baseHit);

				// TODO Is getting attach actor the best way to get Moi from blueprint spawned windows and doors?
				// AActor* attachedActor = baseHit.Actor->GetOwner();

				if (baseHit.Actor->IsA(AModumateObjectInstanceParts_CPP::StaticClass()))
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

					const FModumateObjectInstance *hitMOI = Document->ObjectFromActor(structuralHit.Actor.Get());
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
		bool validWorldSnap = false;
		switch (sketchHit.SnapType)
		{
			case ESnapType::CT_CUSTOMSNAPX:
			case ESnapType::CT_CUSTOMSNAPY:
			case ESnapType::CT_CUSTOMSNAPZ:
			case ESnapType::CT_WORLDSNAPX:
			case ESnapType::CT_WORLDSNAPY:
			case ESnapType::CT_WORLDSNAPZ:
			case ESnapType::CT_WORLDSNAPXY:
				validWorldSnap = true;
				break;
		};

		bool validFaceOverride = ((structuralHit.SnapType == ESnapType::CT_FACESELECT) || (structuralHit.SnapType == ESnapType::CT_NOSNAP));
		bool worldSnapOverride = validWorldSnap && validFaceOverride;

		if (structuralHit.Valid && !worldSnapOverride)
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
		else if (sketchDist < (structuralDist - StructuralSnapPreferenceEpsilon) || worldSnapOverride)
		{
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

	if (projectedHit.Valid)
	{
		if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("HAVE PROJECTED")); }
		EMPlayerState->SnappedCursor.bValid = true;
		EMPlayerState->SnappedCursor.WorldPosition = projectedHit.Location;
		EMPlayerState->SnappedCursor.Actor = projectedHit.Actor.Get();
		ProjectWorldLocationToScreen(projectedHit.Location, EMPlayerState->SnappedCursor.ScreenPosition);
		EMPlayerState->SnappedCursor.HasProjectedPosition = true;
		EMPlayerState->SnappedCursor.ProjectedPosition = baseHit.Location;
		EMPlayerState->SnappedCursor.SnapType = baseHit.SnapType;
		EMPlayerState->SnappedCursor.HitNormal = baseHit.Normal;
		EMPlayerState->SnappedCursor.HitTangent = baseHit.EdgeDir;
		EMPlayerState->SnappedCursor.CP1 = baseHit.CP1;
		EMPlayerState->SnappedCursor.CP2 = baseHit.CP2;
	}
	else
	{
		EMPlayerState->SnappedCursor.HasProjectedPosition = false;
		if (EMPlayerState->ShowDebugSnaps) { GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("NOT PROJECTED")); }
		if (baseHit.Valid)
		{
			EMPlayerState->SnappedCursor.bValid = true;
			EMPlayerState->SnappedCursor.WorldPosition = baseHit.Location;
			EMPlayerState->SnappedCursor.Actor = baseHit.Actor.Get();
			EMPlayerState->SnappedCursor.SnapType = baseHit.SnapType;
			EMPlayerState->SnappedCursor.HitNormal = baseHit.Normal;
			EMPlayerState->SnappedCursor.HitTangent = baseHit.EdgeDir;
			EMPlayerState->SnappedCursor.CP1 = baseHit.CP1;
			EMPlayerState->SnappedCursor.CP2 = baseHit.CP2;
			ProjectWorldLocationToScreen(baseHit.Location, EMPlayerState->SnappedCursor.ScreenPosition);
		}
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
	}

	FModumateObjectInstance *newHoveredObject = nullptr;

	if (InteractionHandle == nullptr && !ShowingModalDialog && actorUnderMouse)
	{
		newHoveredObject = Document->ObjectFromActor(actorUnderMouse);
	}

	// Make sure hovered object is always in a valid group view
	newHoveredObject = EMPlayerState->GetValidHoveredObjectInView(newHoveredObject);
	EMPlayerState->SetHoveredObject(newHoveredObject);

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
}

FModumateFunctionParameterSet AEditModelPlayerController_CPP::ModumateCommand(const FModumateCommand &cmd)
{
	UModumateGameInstance* gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	return gameInstance->DoModumateCommand(cmd);
}

bool AEditModelPlayerController_CPP::ValidateVirtualHit(const FVector &MouseOrigin, const FVector &MouseDir,
	const FVector &HitPoint, float CurObjectHitDist, float CurVirtualHitDist, float MaxScreenDist, float &OutRayDist) const
{
	// The virtual hit point needs to be within a maximum screen distance, and ahead of the mouse
	OutRayDist = (HitPoint - MouseOrigin) | MouseDir;
	float screenSpaceDist = DistanceBetweenWorldPointsInScreenSpace(HitPoint, MouseOrigin);
	if ((screenSpaceDist < MaxScreenDist) && (OutRayDist > KINDA_SMALL_NUMBER) && (OutRayDist < CurVirtualHitDist))
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

bool AEditModelPlayerController_CPP::FindBestMousePointHit(const TArray<FVector> &Points,
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

bool AEditModelPlayerController_CPP::FindBestMouseLineHit(const TArray<TPair<FVector, FVector>> &Lines,
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

FMouseWorldHitType AEditModelPlayerController_CPP::GetAffordanceHit(const FVector &mouseLoc, const FVector &mouseDir, const FAffordanceFrame &affordance, bool allowZSnap) const
{
	FMouseWorldHitType ret;
	ret.Valid = false;
	ret.Actor = nullptr;
	ret.SnapType = ESnapType::CT_NOSNAP;

	FPlane sketchPlane(affordance.Origin, affordance.Normal);

	int32 affordanceDimensions = allowZSnap ? 3 : 2;

	// If we're near the affordance origin, snap to it
	if (DistanceBetweenWorldPointsInScreenSpace(mouseLoc, affordance.Origin) < SnapPointMaxScreenDistance)
	{
		ret.Valid = true;
		ret.SnapType = ESnapType::CT_WORLDSNAPXY;
		ret.Location = affordance.Origin;
	}
	// Otherwise, if we have a custom affordance direction (purple axes), prefer those .. affordance.Tangent will be ZeroVector otherwise
	else if (!affordance.Tangent.IsNearlyZero())
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
				if (DistanceBetweenWorldPointsInScreenSpace(affordanceIntercept, mouseIntercept) < SnapLineMaxScreenDistance)
				{
					// If this is the first hit or closer than the last one...
					if (!ret.Valid || (affordanceIntercept - mouseLoc).Size() < (ret.Location - mouseLoc).Size())
					{
						ret.SnapType = customSnaps[i];
						ret.Location = affordanceIntercept;
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
FMouseWorldHitType AEditModelPlayerController_CPP::GetSketchPlaneMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const
{
	// Tool modes and handles determine for themselves if they want z-axis affordances (off sketch plane)

	/*
	Sketch plane hit rules:
	First, see if we want to snap to any of the basis vectors originating at the affordance. 
	If we receive no basis vector snap, then project the mouse ray onto the sketch plane to determine the hit
	Assumption: the affordance origin is on the sketch plane
	*/
	FMouseWorldHitType ret = GetAffordanceHit(mouseLoc,mouseDir,EMPlayerState->SnappedCursor.AffordanceFrame, EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap);
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
	else
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

FMouseWorldHitType AEditModelPlayerController_CPP::GetUserSnapPointMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const
{
	FMouseWorldHitType ret;
	ret.Valid = false;
	ret.Actor = nullptr;
	ret.SnapType = ESnapType::CT_NOSNAP;

	float bestDir = 0;
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
			float d = DistanceBetweenWorldPointsInScreenSpace(mouseLoc, trySnap.Location);
			if (ret.Valid || d < bestDir)
			{
				bestDir = d;
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
FMouseWorldHitType AEditModelPlayerController_CPP::GetObjectMouseHit(const FVector &mouseLoc, const FVector &mouseDir, bool bCheckSnapping) const
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

		const FModumateObjectInstance* moi = Document->ObjectFromActor(objectHit.Actor.Get());
		// TODO: this should be an interface function or there should be a way to find the planar object types
		if (moi && moi->GetObjectType() == EObjectType::OTMetaPlane)
		{
			FVector moiNormal = moi->GetNormal();
			FPlane plane = FPlane(moi->GetObjectLocation(), moiNormal);
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
			FModumateObjectInstance *bestObj = Document->GetObjectById(bestPoint.ObjID);
			if (bestObj)
			{
				objectHit.Valid = true;
				objectHit.Location = bestPoint.Point;
				objectHit.EdgeDir = bestPoint.Direction;
				objectHit.Actor = bestObj->GetActor();
				objectHit.SnapType = (bestPoint.CP2 == INDEX_NONE) ? ESnapType::CT_CORNERSNAP : ESnapType::CT_MIDSNAP;
				objectHit.CP1 = bestPoint.CP1;
				objectHit.CP2 = bestPoint.CP2;
				if (objectHit.Actor == directHitActor)
				{
					objectHit.Normal = directHitNormal;
				}
			}
		}
		else if (FindBestMouseLineHit(CurHitLineLocations, mouseLoc, mouseDir, objectHitDist, bestVirtualHitIndex, bestLineIntersection, bestVirtualHitDist))
		{
			FStructureLine &bestLine = SnappingView->LineSegments[bestVirtualHitIndex];
			FModumateObjectInstance *bestObj = Document->GetObjectById(bestLine.ObjID);
			if (bestObj)
			{
				objectHit.Valid = true;
				objectHit.Location = bestLineIntersection;
				objectHit.EdgeDir = (bestLine.P2 - bestLine.P1).GetSafeNormal();
				objectHit.Actor = bestObj->GetActor();
				objectHit.SnapType = ESnapType::CT_EDGESNAP;
				objectHit.CP1 = bestLine.CP1;
				objectHit.CP2 = bestLine.CP2;
				if (objectHit.Actor == directHitActor)
				{
					objectHit.Normal = directHitNormal;
				}
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
		for (FModumateObjectInstance *moi : Document->GetObjectInstances())
		{
			if (moi && moi->IsCollisionEnabled() && moi->UseStructureDataForCollision())
			{
				moi->GetStructuralPointsAndLines(tempPointsForCollision, tempLinesForCollision, false, false);

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

	// TODO: we're defaulting to the XY sketch plane but we should disambiguate between candidate normals
	if (objectHit.Normal.IsNearlyZero())
	{
		objectHit.Normal = FVector::UpVector;
	}

	return objectHit;
}

FMouseWorldHitType AEditModelPlayerController_CPP::UpdateHandleHit(const FVector &mouseLoc, const FVector &mouseDir)
{
	FMouseWorldHitType handleHit;

	// Check adjustment handle under cursor, return no snap if true.
	// Also take this opportunity to maintain the current hover handle.
	AAdjustmentHandleActor_CPP* newHoverHandle = nullptr;
	FHitResult hitSingleResult;

	if (GetWorld()->LineTraceSingleByObjectType(hitSingleResult, mouseLoc, mouseLoc + MaxRaycastDist * mouseDir, HandleTraceObjectQueryParams, HandleTraceQueryParams))
	{
		newHoverHandle = Cast<AAdjustmentHandleActor_CPP>(hitSingleResult.Actor);
	}

	if (HoverHandle != newHoverHandle)
	{
		if (HoverHandle)
		{
			HoverHandle->OnHover(false);
		}

		HoverHandle = newHoverHandle;

		if (HoverHandle)
		{
			HoverHandle->OnHover(true);
		}
	}

	if (newHoverHandle)
	{
		handleHit.Valid = true;
		handleHit.Actor = newHoverHandle;
		handleHit.SnapType = ESnapType::CT_NOSNAP;
	}

	return handleHit;
}

/*
Support function: given any of the hits (structural, object or sketch), if the user is holding shift, project either onto the custom affordance or the snapping span (axis aligned)
*/
FMouseWorldHitType AEditModelPlayerController_CPP::GetShiftConstrainedMouseHit(const FMouseWorldHitType &baseHit) const
{
	FMouseWorldHitType ret;
	ret.Valid = false;
	ret.Actor = nullptr;

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

FLinearColor AEditModelPlayerController_CPP::GetSnapAffordanceColor(const FAffordanceLine &a)
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

bool AEditModelPlayerController_CPP::AddSnapAffordance(const FVector &startLoc, const FVector &endLoc, const FLinearColor &overrideColor) const
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

void AEditModelPlayerController_CPP::AddSnapAffordancesToOrigin(const FVector &origin, const FVector &hitLocation) const
{
	FVector fromDim = hitLocation;
	FVector toDim = FVector(fromDim.X, fromDim.Y, origin.Z);

	AddSnapAffordance(fromDim, toDim);

	fromDim = toDim;
	toDim = origin;

	AddSnapAffordance(fromDim, toDim);
}

bool AEditModelPlayerController_CPP::GetCursorFromUserSnapPoint(const FTransform &snapPoint, FVector &outCursorPosFlat, FVector &outHeightDelta) const
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

bool AEditModelPlayerController_CPP::HasUserSnapPointAtPos(const FVector &snapPointPos, float tolerance) const
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

/*
Called after UpdateMouseHits, this function draws affordances based on the data in the snapped cursor
*/
void AEditModelPlayerController_CPP::UpdateAffordances() const
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

void AEditModelPlayerController_CPP::UpdateUserSnapPoint()
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

void AEditModelPlayerController_CPP::SetSelectionMode(ESelectObjectMode NewSelectionMode)
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

void AEditModelPlayerController_CPP::GroupSelected(bool makeGroup)
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
			[](const FModumateObjectInstance *ob)
			{
				return ob->GetObjectType() == EObjectType::OTGroup;
			},
			[](const FModumateObjectInstance *ob) 
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

bool AEditModelPlayerController_CPP::ToggleGravityPawn()
{
	// Get required pawns
	APlayerCameraManager* camManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	APawn* currentPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AEditModelGameMode_CPP *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
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
			EMToggleGravityPawn = GetWorld()->SpawnActor<AEditModelToggleGravityPawn_CPP>(gameMode->ToggleGravityPawnClass, possessLocation, yawRotOnly, spawnParam);
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

		// Move the pawn location forward 
		FVector forwardOffset = possessRotation.Vector() * 600.f;
		FVector pawnActorLocation = possessLocation + forwardOffset;
		
		FTransform playerActorTransform = FTransform(possessRotation, pawnActorLocation, FVector::OneVector);
		FTransform cameraTransform = FTransform(possessRotation, possessLocation, FVector::OneVector);
		EMPlayerPawn->SetCameraTransform(playerActorTransform, cameraTransform, possessRotation);
	}
	return true;
}
