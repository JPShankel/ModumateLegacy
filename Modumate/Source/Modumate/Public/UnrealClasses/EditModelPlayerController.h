// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "Database/ModumateObjectEnums.h"
#include "Math/Sphere.h"

#include "EditModelPlayerController.generated.h"

/**
 *
 */

class AAdjustmentHandleActor;
class AEditModelPlayerState;
enum class EModumatePermission : uint8;
class UEditModelCameraController;
class UEditModelInputAutomation;
class UEditModelInputHandler;
class UHUDDrawWidget;
class UModumateDocument;
class AModumateObjectInstance;
class FModumateSnappingView;

UENUM(BlueprintType)
enum class ECameraMode : uint8
{
	None,
	Orbit,
	Pan
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUserSnapPointEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToolModeChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToolCreateObjectModeChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToolAssemblyChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoundInputActionEvent, FName, ActionName, EInputEvent, InputEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoundInputAxisEvent, FName, AxisName, float, Value);
DECLARE_DELEGATE_TwoParams(FBoundInputActionEventDelegate, FName, EInputEvent);
DECLARE_DELEGATE_TwoParams(FBoundInputAxisEventDelegate, FName, float);

UENUM()
enum class EInputMovementAxes : uint8
{
	MoveYaw,
	MovePitch,
	MoveForward,
	MoveRight,
	MoveUp
};

UCLASS(Config=Game)
class MODUMATE_API AEditModelPlayerController : public APlayerController
{
	GENERATED_BODY()

	AEditModelPlayerController();
	virtual ~AEditModelPlayerController();
	virtual void PostInitializeComponents() override;

private:

	UPROPERTY()
	UModumateDocument* Document;

	FModumateSnappingView *SnappingView;

	TSet<int32> SnappingIDsToIgnore;
	TArray<AActor*> SnappingActorsToIgnore;

	FCollisionObjectQueryParams MOITraceObjectQueryParams, HandleTraceObjectQueryParams;
	FCollisionQueryParams MOITraceQueryParams, HandleTraceQueryParams;

	static FLinearColor GetSnapAffordanceColor(const FAffordanceLine &a);
	bool AddSnapAffordance(const FVector &startLoc, const FVector &endLoc, const FLinearColor &overrideColor = FLinearColor::Transparent) const;
	void AddSnapAffordancesToOrigin(const FVector &origin, const FVector &hitLocation) const;
	bool GetCursorFromUserSnapPoint(const FTransform &snapPoint, FVector &outCursorPosFlat, FVector &outHeightDelta) const;
	bool HasUserSnapPointAtPos(const FVector &snapPointPos, float tolerance = KINDA_SMALL_NUMBER) const;

	void UpdateMouseHits(float DeltaTime);
	void UpdateUserSnapPoint();

	FTimerHandle ControllerTimer;
	FGuid TelemetrySessionKey;
	FDateTime SessionStartTime;
	static const FString InputTelemetryDirectory;

	bool StartTelemetrySession(bool bRecordLoadedDocument);
	bool EndTelemetrySession(bool bAsyncUpload = true);
	bool UploadInputTelemetry(bool bAsynchronous = true) const;

	bool SnapDistAlongAffordance(FVector& SnappedPosition, const FVector& AffordanceOrigin, const FVector& AffordanceDir) const;
	bool ValidateVirtualHit(const FVector &MouseOrigin, const FVector &MouseDir, const FVector &HitPoint,
		float CurObjectHitDist, float CurVirtualHitDist, float MaxScreenDist, float &OutRayDist) const;
	bool FindBestMousePointHit(const TArray<FVector> &Points, const FVector &MouseOrigin, const FVector &MouseDir, float CurObjectHitDist, int32 &OutBestIndex, float &OutBestRayDist) const;
	bool FindBestMouseLineHit(const TArray<TPair<FVector, FVector>> &Lines, const FVector &MouseOrigin, const FVector &MouseDir, float CurObjectHitDist, int32 &OutBestIndex, FVector &OutBestIntersection, float &OutBestRayDist) const;
	FMouseWorldHitType GetAffordanceHit(const FVector &mouseLoc, const FVector &mouseDir, const FAffordanceFrame &affordance, bool allowZSnap) const;

	FMouseWorldHitType GetShiftConstrainedMouseHit(const FMouseWorldHitType &baseHit) const;
	FMouseWorldHitType GetObjectMouseHit(const FVector &mouseLoc, const FVector &mouseDir, bool bCheckSnapping) const;
	FMouseWorldHitType GetSketchPlaneMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const;
	FMouseWorldHitType GetUserSnapPointMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const;

	// These are cached helpers for GetObjectMouseHit, to avoid allocating new TArrays on each call for GetObjectMouseHit.
	mutable TArray<AModumateObjectInstance *> CurHitPointMOIs;
	mutable TArray<FVector> CurHitPointLocations;
	mutable TArray<AModumateObjectInstance *> CurHitLineMOIs;
	mutable TArray<TPair<FVector, FVector>> CurHitLineLocations;

protected:
	virtual void SetupInputComponent() override;

	void CreateTools();

	template<class T>
	TScriptInterface<IEditModelToolInterface> CreateTool()
	{
		UObject *toolObject = NewObject<T>(this);
		return TScriptInterface<IEditModelToolInterface>(toolObject);
	}

	void RegisterTool(TScriptInterface<IEditModelToolInterface> NewTool);

	FDateTime TimeOfLastAutoSave, TimeOfLastUpload;
	bool bWantAutoSave = false;
	bool bCurProjectAutoSaves = false;
	bool bWantTelemetryUpload = false;

	// Keep track of the most recent snapped cursor
	UPROPERTY()
	FSnappedCursor LastSnappedCursor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (Category = Snap, ToolTip = "How many seconds have elapsed since we have started the entire process of automatically creating a user snap point."))
	float UserSnapAutoCreationElapsed;

	// How long the cursor must be held on the same snap point before a UserSnapPoint begins to animate,
	// and how long it does animate, before being created.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Category = Snap, ToolTip = "How long to wait before starting the animation of creating a user snap point."))
	float UserSnapAutoCreationDelay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Category = Snap, ToolTip = "How long the automatic user snap point creation animation should last before the point is created."))
	float UserSnapAutoCreationDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Category = Snap, ToolTip = "The transform of the current user snap point that is being automatically created."))
	FTransform PendingUserSnapPoint;

	UPROPERTY(VisibleAnywhere, BlueprintAssignable, meta = (Category = Snap, ToolTip = "The event that fires when we start animating a user snap point being automatically created."))
	FUserSnapPointEvent OnStartCreateUserSnapPoint;

	UPROPERTY(VisibleAnywhere, BlueprintAssignable, meta = (Category = Snap, ToolTip = "The event that fires when we cancel the automatic creation of a user snap point."))
	FUserSnapPointEvent OnCancelCreateUserSnapPoint;

	UPROPERTY(VisibleAnywhere, BlueprintAssignable, meta = (Category = Snap, ToolTip = "The event that fires when we successfully finish the automatic creation of a user snap point."))
	FUserSnapPointEvent OnFinishCreateUserSnapPoint;

	UFUNCTION()
	void OnToggleFullscreen(bool bIsFullscreen);

public:

	UPROPERTY()
	FOnToolModeChanged OnToolModeChanged;

	UPROPERTY()
	FOnToolCreateObjectModeChanged OnToolCreateObjectModeChanged;

	UPROPERTY()
	FOnToolAssemblyChanged OnToolAssemblyChanged;

	UPROPERTY()
	FOnBoundInputActionEvent HandledInputActionEvent;

	UPROPERTY()
	FOnBoundInputAxisEvent HandledInputAxisEvent;

	void OnHandledInputActionName(FName ActionName, EInputEvent InputEvent);
	void OnHandledInputAxisName(FName AxisName, float Value);

	template<typename TEnum>
	void OnHandledInputAction(TEnum ActionEnum, EInputEvent InputEvent)
	{
		OnHandledInputActionName(GetEnumValueShortName(ActionEnum), InputEvent);
	}

	template<typename TEnum>
	void OnHandledInputAxis(TEnum AxisEnum, float Value)
	{
		OnHandledInputAxisName(GetEnumValueShortName(AxisEnum), Value);
	}

	bool DistanceBetweenWorldPointsInScreenSpace(const FVector &Point1, const FVector &Point2, float &OutScreenDist) const;
	bool GetScreenScaledDelta(const FVector &Origin, const FVector &Normal, const float DesiredWorldDist, const float MaxScreenDist,
		FVector &OutWorldPos, FVector2D &OutScreenPos) const;

	// TODO: this should not need to exist if the PlayerController can handle all desires to simulate cursor hit results at positions other than the current mouse position.
	// (unified HandleInputNumber and refactored UserSnapPoint(s) need to be addressed in order for this to be the case)
	FMouseWorldHitType GetSimulatedStructureHit(const FVector& HitTarget) const;

	UPROPERTY()
	AEditModelPlayerState *EMPlayerState;

	UPROPERTY()
	class AEditModelPlayerPawn *EMPlayerPawn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Pawn)
	class AEditModelToggleGravityPawn *EMToggleGravityPawn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class ASkyActor* SkyActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class AAxesActor* AxesActor;

	void BeginWithPlayerState();

	// Event overrides
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float deltaSeconds) override;

	void TickInput(float DeltaTime);

	void AddAllOriginAffordances() const;
	void SetObjectSelected(AModumateObjectInstance *ob, bool bSelected, bool bDeselectOthers);
	void SetObjectsSelected(TSet<AModumateObjectInstance*>& Obs, bool bSelected, bool bDeselectOthers);

	UFUNCTION()
	void OnControllerTimer();

	void HandleDigitKey(int32 DigitKey);

	// Raw mouse input handling functions, so that C++ owns the primary bindings.
	UFUNCTION(Category = Input)
	void HandleRawLeftMouseButtonPressed();
	UFUNCTION(Category = Input)
	void HandleRawLeftMouseButtonReleased();

	// Logical mouse input handling events
	// TODO: They are BlueprintNativeEvents for now, so that Blueprint can still authoritatively
	//       handle how (and if) they are passed along to UI, this PlayerController (OnLButtonDown, etc.).
	//       Eventually that logic should be ported to these functions, these should no longer be BlueprintNativeEvents,
	//       and these should replace those other OnLButtonDown functions.
	UFUNCTION(BlueprintNativeEvent, Category = Input)
	void HandleLeftMouseButton(bool bPressed);

	UFUNCTION(BlueprintCallable, Category = Selection)
	void SelectAll();

	UFUNCTION(BlueprintCallable, Category = Selection)
	void SelectInverse();

	UFUNCTION(BlueprintCallable, Category = Selection)
	void DeselectAll();

	UFUNCTION(Category = Selection)
	FTransform MakeUserSnapPointFromCursor(const FSnappedCursor &cursor) const;

	UFUNCTION(Category = Selection)
	bool CanMakeUserSnapPointAtCursor(const FSnappedCursor &cursor) const;

	UFUNCTION(Category = Selection)
	void ToggleUserSnapPoint();

	UFUNCTION(Category = Selection)
	void AddUserSnapPoint(const FTransform &snapPointTransform);

	UFUNCTION(Category = Selection)
	void ClearUserSnapPoints();

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite)
	bool bAllowDebugCrash = false;

	UFUNCTION()
	void DebugCrash();

	EAppReturnType::Type ShowMessageBox(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption);

	bool CanShowFileDialog();

	UFUNCTION()
	void CleanSelectedObjects();

	void SetViewGroupObject(const AModumateObjectInstance *ob);

	Modumate::FModumateFunctionParameterSet ModumateCommand(const Modumate::FModumateCommand &cmd);

	UModumateDocument *GetDocument() const { return Document; }
	FModumateSnappingView *GetSnappingView() const { return SnappingView; }

	/*  Assembly Layer Inputs */
	UFUNCTION(BlueprintCallable, Category = Assembly)
	void UpdateDefaultWallHeight(float newHeight);

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultWallHeightFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	void UpdateDefaultWindowHeightWidth(float newHeight, float newWidth);

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultWindowHeightFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultRailingsHeightFromDoc() const;

	// Layer Justification
	UFUNCTION(BlueprintCallable, Category = Justification)
	void UpdateDefaultJustificationZ(float newValue);

	UFUNCTION(BlueprintCallable, Category = Justification)
	void UpdateDefaultJustificationXY(float newValue);

	UFUNCTION(BlueprintCallable, Category = Justification)
	float GetDefaultJustificationZ() const;

	UFUNCTION(BlueprintCallable, Category = Justification)
	float GetDefaultJustificationXY() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	void ToolAbortUse();

	UFUNCTION(Category = Snap)
	void IgnoreObjectIDForSnapping(int32 ObjectID);

	UFUNCTION(Category = Snap)
	void ClearIgnoredIDsForSnapping();

	UFUNCTION(Category = Snap)
	void UpdateMouseTraceParams();

	UFUNCTION(BlueprintPure, Category = Snap)
	bool GetActiveUserSnapPoint(FTransform &outSnapPoint) const;

	UFUNCTION()
	bool LineTraceSingleAgainstMOIs(struct FHitResult& OutHit, const FVector& Start, const FVector& End) const;

	UFUNCTION(BlueprintPure)
	bool IsCursorOverWidget() const;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = HUD)
	void ClearTextInputs();

	UFUNCTION(BlueprintCallable, DisplayName = "Set Field of View", meta = (ScriptMethod = "SetFieldOfView"))
	void SetFieldOfViewCommand(float FieldOfView);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FBox TestBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	bool ShowingModalDialog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	bool CameraInputLock = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Cursor)
	bool bIsCursorAtAnyWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Cursor)
	ESelectObjectMode SelectionMode;

	UFUNCTION(BlueprintCallable, Category = Cursor)
	void SetSelectionMode(ESelectObjectMode NewSelectionMode);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	float MaxRaycastDist;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Snap)
	TArray<FTransform> UserSnapPoints;

	UFUNCTION(BlueprintCallable, Category = Modeling)
	void GroupSelected(bool makeGroup);

	// Calculate the location needed to view the boundary set by TargetSphere
	FVector CalculateViewLocationForSphere(const FSphere &TargetSphere, const FVector &ViewVector, float AspectRatio, float FOV);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD, meta = (DeprecatedProperty))
	UHUDDrawWidget* HUDDrawWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	UTexture2D* StaticCamTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Category = Snap, ToolTip = "The distance bias at which we will prefer hits against virtual objects (snaps, MOIs that are just points/lines) to direct hits."))
	float VirtualHitBias = 0.5f;

	// The difference in camera distance, in cm, that a structural snap hit
	// will be preferred compared to the sketch plane in front of it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Snap)
	float StructuralSnapPreferenceEpsilon = 10.0f;

	// The maximum distance, in pixels, between the cursor in screen space and a valid point snap location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Snap)
	float SnapPointMaxScreenDistance = 15.0f;

	// The maximum distance, in pixels, between the cursor in screen space and a valid line snap location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Snap)
	float SnapLineMaxScreenDistance = 10.0f;

	// The maximum distance, in cm, between a raycast used for snapping and geometry that would occlude it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Snap)
	float SnapOcclusionEpsilon = 1.0f;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UUserWidget> SoftwareCursorWidgetClass;

	UPROPERTY()
	UUserWidget* SoftwareCursorWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	UEditModelInputAutomation* InputAutomationComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	UEditModelInputHandler* InputHandlerComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	UEditModelCameraController* CameraController;

	UPROPERTY()
	TMap<EToolMode, TScriptInterface<IEditModelToolInterface>> ModeToTool;

	UPROPERTY()
	TScriptInterface<IEditModelToolInterface> CurrentTool;

	UPROPERTY()
	AAdjustmentHandleActor *InteractionHandle;

	UPROPERTY()
	TArray<AAdjustmentHandleActor *> HoverHandleActors;

	UPROPERTY()
	AAdjustmentHandleActor *HoverHandleActor;

	UPROPERTY()
	float CurrentToolUseDuration;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<EEditViewModes> ValidViewModes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EEditViewModes> DefaultViewModes;

	UFUNCTION()
	bool GetRequiredViewModes(EToolCategories ToolCategory, TArray<EEditViewModes>& OutViewModes) const;

	UFUNCTION(BlueprintPure, Category = Tools)
	bool ToolIsInUse() const;

	UFUNCTION(BlueprintPure, Category = Tools)
	EToolMode GetToolMode();

	UFUNCTION()
	void SetToolMode(EToolMode NewToolMode);

	UFUNCTION(BlueprintCallable, Category = Tools)
	void AbortUseTool();

	UFUNCTION(BlueprintCallable, Category = Tools)
	void SetToolCreateObjectMode(EToolCreateObjectMode CreateObjectMode);

	UFUNCTION(BlueprintCallable, Category = Tools)
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY()
	bool bResetFocusToGameViewport = false;

	// Input handling
	void OnHoverHandleWidget(class UAdjustmentHandleWidget* HandleWidget, bool bIsHovered);
	void OnPressHandleWidget(class UAdjustmentHandleWidget* HandleWidget, bool bIsPressed);

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnLButtonDown();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnLButtonUp();

	bool IsShiftDown() const;
	bool IsControlDown() const;

	void HandleUndo();
	void HandleRedo();

	UFUNCTION()
	bool HandleInvert();

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	bool HandleEscapeKey();

	bool HandleInputNumber(double inputNumber);

	UFUNCTION(BlueprintPure)
	class AEditModelPlayerHUD* GetEditModelHUD() const;

	// Persistence
	UFUNCTION(BlueprintCallable, Category = Persistence)
	bool SaveModelAs();

	UFUNCTION(BlueprintCallable, Category = Persistence)
	bool SaveModel();
	bool SaveModelFilePath(const FString &filepath);

	UFUNCTION(BlueprintCallable, Category = Persistence)
	bool LoadModel(bool bLoadOnlyDeltas = false);
	bool LoadModelFilePath(const FString &filename, bool bSetAsCurrentProject, bool bAddToRecents, bool bEnableAutoSave, bool bLoadOnlyDeltas = false);

	UFUNCTION(BlueprintCallable, Category = Persistence)
	void NewModel(bool bShouldCheckForSave);

	UFUNCTION(BlueprintCallable, Category = Persistence)
	bool CheckSaveModel();

	bool OnSavePDF();
	bool OnCreateDwg();
	bool OnCreateQuantitiesCsv(const TFunction<void(FString, bool)>& UsageNotificationCallback = nullptr);

	// Check user plan & permission, and if user doesn't have sufficient status then show a modal dialog and return false.
	// The modal dialog has an optional confirm button and confirmation callback.
	bool CheckUserPlanAndPermission(EModumatePermission Permission, const FText& NoPermissionText = FText::GetEmpty(),
		const FText& ConfirmText = FText::GetEmpty(), const TFunction<void()>& ConfirmCallback = nullptr);

	UFUNCTION(BlueprintCallable)
	bool TakeScreenshot();

	UFUNCTION()
	void DeleteActionDefault();

	UFUNCTION()
	void DeleteActionOnlySelected();

	UFUNCTION(BlueprintCallable)
	void SetThumbnailCapturer(class ASceneCapture2D* InThumbnailCapturer);

	UFUNCTION(BlueprintCallable)
	bool CaptureProjectThumbnail();

	UFUNCTION(BlueprintCallable)
	bool GetScreenshotFileNameWithDialog(FString& Filepath, FString &Filename);

	UPROPERTY()
	class ASceneCapture2D* ThumbnailCapturer;

	UFUNCTION(BlueprintCallable, Category = CameraViewMenu)
	bool ToggleGravityPawn();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Menu)
	TSubclassOf<class UEditModelUserWidget> EditModelUserWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Menu)
	TSubclassOf<class ADynamicIconGenerator> DynamicIconGeneratorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Menu)
	class UEditModelUserWidget *EditModelUserWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Menu)
	class ADynamicIconGenerator *DynamicIconGenerator;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "CutPlane")
	UMaterialParameterCollection* CutPlaneCullingMaterialCollection;

	int32 CurrentCullingCutPlaneID = MOD_ID_NONE;

	void SetCurrentCullingCutPlane(int32 ObjID = MOD_ID_NONE, bool bRefreshMenu = true);
	void UpdateCutPlaneCullingMaterialInst(int32 ObjID = MOD_ID_NONE);
	void ToggleAllCutPlanesColor(bool bEnable);
	FPlane GetCurrentCullingPlane() const;

	bool bBeganWithPlayerState = false;
};
