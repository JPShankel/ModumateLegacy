// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "Database/ModumateObjectEnums.h"
#include "Math/Sphere.h"

#include "EditModelPlayerController_CPP.generated.h"

/**
 *
 */

class AAdjustmentHandleActor;
class AEditModelPlayerState_CPP;
class UEditModelCameraController;
class UEditModelInputAutomation;
class UEditModelInputHandler;
class UHUDDrawWidget;
class FModumateDocument;
class FModumateObjectInstance;
class FModumateSnappingView;

UENUM(BlueprintType)
enum class ECameraMode : uint8
{
	None,
	Orbit,
	Pan
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUserSnapPointEvent);

UCLASS(Config=Game)
class MODUMATE_API AEditModelPlayerController_CPP : public APlayerController
{
	GENERATED_BODY()

	AEditModelPlayerController_CPP();
	virtual ~AEditModelPlayerController_CPP();
	virtual void PostInitializeComponents() override;

private:

	FModumateDocument *Document;
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
	mutable TArray<FModumateObjectInstance *> CurHitPointMOIs;
	mutable TArray<FVector> CurHitPointLocations;
	mutable TArray<FModumateObjectInstance *> CurHitLineMOIs;
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

	FDateTime TimeOfLastAutoSave;
	bool WantAutoSave = false;

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

public:

	bool DistanceBetweenWorldPointsInScreenSpace(const FVector &Point1, const FVector &Point2, float &OutScreenDist) const;
	bool GetScreenScaledDelta(const FVector &Origin, const FVector &Normal, const float DesiredWorldDist, const float MaxScreenDist,
		FVector &OutWorldPos, FVector2D &OutScreenPos) const;

	UPROPERTY()
	AEditModelPlayerState_CPP *EMPlayerState;

	UPROPERTY()
	class AEditModelPlayerPawn_CPP *EMPlayerPawn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Pawn)
	class AEditModelToggleGravityPawn_CPP *EMToggleGravityPawn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class ASkyActor *SkyActor;

	// Event overrides
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float deltaSeconds) override;

	void TickInput(float DeltaTime);

	void AddAllOriginAffordances() const;
	void SetObjectSelected(const FModumateObjectInstance *ob, bool selected);

	UFUNCTION()
	void OnControllerTimer();

	// Override the base APlayerController::InputKey function, for input recording,
	// and to make sure all physical inputs get forwarded to wherever they need to go
	// (regardless of whether they're handled in C++, directly in Blueprint as keys,
	//  indirectly as FInputChords, interpreted as actions, etc.)
	virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

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

	UFUNCTION(BlueprintCallable, Category = Selection)
	bool SelectObjectById(int32 ObjectID);

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

	UFUNCTION()
	void CleanSelectedObjects();

	void SetViewGroupObject(const FModumateObjectInstance *ob);

	Modumate::FModumateFunctionParameterSet ModumateCommand(const Modumate::FModumateCommand &cmd);

	FModumateDocument *GetDocument() const { return Document; }
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visibility)
	bool bCutPlaneVisible = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input)
	UEditModelInputAutomation* InputAutomationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Input)
	UEditModelInputHandler* InputHandlerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
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

	UFUNCTION(BlueprintPure, Category = Tools)
	bool ToolIsInUse() const;

	UFUNCTION(BlueprintPure, Category = Tools)
	EToolMode GetToolMode();

	UFUNCTION()
	void SetToolMode(EToolMode NewToolMode);

	UFUNCTION(BlueprintCallable, Category = Tools)
	void AbortUseTool();

	UFUNCTION(BlueprintCallable, Category = Tools)
	void SetToolAxisConstraint(EAxisConstraint AxisConstraint);

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
	bool LoadModel();
	bool LoadModelFilePath(const FString &filename, bool addToRecents = true);

	UFUNCTION(BlueprintCallable, Category = Persistence)
	void NewModel();

	UFUNCTION(BlueprintCallable, Category = Persistence)
	bool CheckSaveModel();

	bool OnSavePDF();
	bool OnCreateDwg();

	UFUNCTION(BlueprintCallable, Category = Persistence)
	void TrySavePDF();

	// Check user plan & permission, show modal dialog if user doesn't have sufficient status and return false
	UFUNCTION(BlueprintCallable, Category = Persistence)
	bool CheckUserPlanAndPermission(EModumatePermission Permission);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
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
	bool GetScreenshotFileNameWithDialog(FString &filename);

	UPROPERTY()
	class ASceneCapture2D* ThumbnailCapturer;

	UFUNCTION(BlueprintCallable)
	void SetCutPlaneVisibility(bool bVisible);

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


};
