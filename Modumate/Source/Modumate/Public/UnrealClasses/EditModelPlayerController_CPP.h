// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EditModelToolInterface.h"
#include "ModumateConsoleCommand.h"
#include "ModumateSnappedCursor.h"
#include "ModumateObjectEnums.h"
#include "Sphere.h"

#include "EditModelPlayerController_CPP.generated.h"

/**
 *
 */

class AAdjustmentHandleActor_CPP;
class AEditModelPlayerState_CPP;
class UDraftingPreviewWidget_CPP;
class UEditModelCameraController;
class UEditModelInputAutomation;
class UEditModelInputHandler;
class UModumateCraftingWidget_CPP;
class UHUDDrawWidget_CPP;
class UModumateDrawingSetWidget_CPP;

namespace Modumate
{
	class FModumateSnappingView;
	class FModumateDocument;
	class FModumateObjectInstance;
}

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
	FMouseWorldHitType GetAffordanceHit(const FVector &mouseLoc,const FVector &mouseDir,const FAffordanceFrame &affordance,bool allowZSnap) const;

private:

	Modumate::FModumateDocument *Document;
	Modumate::FModumateSnappingView *SnappingView;

	TSet<int32> SnappingIDsToIgnore;
	TArray<AActor*> SnappingActorsToIgnore;

	FCollisionObjectQueryParams MOITraceObjectQueryParams, HandleTraceObjectQueryParams;
	FCollisionQueryParams MOITraceQueryParams, HandleTraceQueryParams;
	 
	FString TextBoxUserInput;

	static FLinearColor GetSnapAffordanceColor(const FAffordanceLine &a);
	bool AddSnapAffordance(const FVector &startLoc, const FVector &endLoc, const FLinearColor &overrideColor = FLinearColor::Transparent) const;
	void AddSnapAffordancesToOrigin(const FVector &origin, const FVector &hitLocation) const;
	bool GetCursorFromUserSnapPoint(const FTransform &snapPoint, FVector &outCursorPosFlat, FVector &outHeightDelta) const;
	bool HasUserSnapPointAtPos(const FVector &snapPointPos, float tolerance = KINDA_SMALL_NUMBER) const;

	void UpdateMouseHits(float DeltaTime);
	void UpdateAffordances() const;
	void UpdateUserSnapPoint();

	FTimerHandle ControllerTimer;

	bool ValidateVirtualHit(const FVector &MouseOrigin, const FVector &MouseDir, const FVector &HitPoint,
		float CurObjectHitDist, float CurVirtualHitDist, float MaxScreenDist, float &OutRayDist) const;
	bool FindBestMousePointHit(const TArray<FVector> &Points, const FVector &MouseOrigin, const FVector &MouseDir, float CurObjectHitDist, int32 &OutBestIndex, float &OutBestRayDist) const;
	bool FindBestMouseLineHit(const TArray<TPair<FVector, FVector>> &Lines, const FVector &MouseOrigin, const FVector &MouseDir, float CurObjectHitDist, int32 &OutBestIndex, FVector &OutBestIntersection, float &OutBestRayDist) const;

	FMouseWorldHitType UpdateHandleHit(const FVector &mouseLoc, const FVector &mouseDir);
	FMouseWorldHitType GetShiftConstrainedMouseHit(const FMouseWorldHitType &baseHit) const;
	FMouseWorldHitType GetObjectMouseHit(const FVector &mouseLoc, const FVector &mouseDir, bool bCheckSnapping) const;
	FMouseWorldHitType GetSketchPlaneMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const;
	FMouseWorldHitType GetUserSnapPointMouseHit(const FVector &mouseLoc, const FVector &mouseDir) const;

	// These are cached helpers for GetObjectMouseHit, to avoid allocating new TArrays on each call for GetObjectMouseHit.
	mutable TArray<Modumate::FModumateObjectInstance *> CurHitPointMOIs;
	mutable TArray<FVector> CurHitPointLocations;
	mutable TArray<Modumate::FModumateObjectInstance *> CurHitLineMOIs;
	mutable TArray<TPair<FVector, FVector>> CurHitLineLocations;

	UPROPERTY()
	UDraftingPreviewWidget_CPP *DraftingPreview;

	UPROPERTY()
	UModumateCraftingWidget_CPP *CraftingWidget;

	UPROPERTY()
	UModumateDrawingSetWidget_CPP *DrawingSetWidget;

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

	float DistanceBetweenWorldPointsInScreenSpace(const FVector &p1, const FVector &p2) const;

	UPROPERTY()
	AEditModelPlayerState_CPP *EMPlayerState;

	UPROPERTY()
	class AEditModelPlayerPawn_CPP *EMPlayerPawn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Pawn)
	class AEditModelToggleGravityPawn_CPP *EMToggleGravityPawn;

	// Event overrides
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float deltaSeconds) override;

	void TickInput(float DeltaTime);

	void SetObjectSelected(const Modumate::FModumateObjectInstance *ob, bool selected);

	UFUNCTION()
	void OnControllerTimer();

	// Override the base APlayerController::InputKey function, for input recording,
	// and to make sure all physical inputs get forwarded to wherever they need to go
	// (regardless of whether they're handled in C++, directly in Blueprint as keys,
	//  indirectly as FInputChords, interpreted as actions, etc.)
	virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

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

	UFUNCTION(BlueprintCallable, Category = Selection)
	bool SelectGroupObject(const AActor* MoiActor);

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite)
	bool bAllowDebugCrash = false;

	UFUNCTION()
	void DebugCrash();

	UFUNCTION()
	void CleanSelectedObjects();

	void SetViewGroupObject(const Modumate::FModumateObjectInstance *ob);

	bool TryMakePrismFromSegments(EObjectType objectType, const FName &assemblyKey, bool inverted, const Modumate::FModumateObjectInstance *startSegment = nullptr);
	bool TryMakeCabinetFromSegments();

	void MakeRailsFromSegments();

	// Draw basic dimension string own by the PlayerController
	void UpdateDimensionString(const FVector &p1, const FVector &p2, const FVector &normal, const int32 groupIndex = 0, const float offset = 50.f);

	Modumate::FModumateFunctionParameterSet ModumateCommand(const Modumate::FModumateCommand &cmd);

	Modumate::FModumateSnappingView *GetSnappingView() const { return SnappingView; }

	/*  Assembly Layer Inputs */
	UFUNCTION(BlueprintCallable, Category = Assembly)
	void UpdateDefaultWallHeight(float newHeight);

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultWallHeightFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	void UpdateDefaultRailHeight(float newHeight);

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultRailHeightFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	void UpdateDefaultCabinetHeight(float newHeight);

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultCabinetHeightFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	void UpdateDefaultDoorHeightWidth(float newHeight, float newWidth);

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultDoorHeightFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultDoorWidthFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	void UpdateDefaultWindowHeightWidth(float newHeight, float newWidth);

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultWindowHeightFromDoc() const;

	UFUNCTION(BlueprintCallable, Category = Assembly)
	float GetDefaultWindowWidthFromDoc() const;

	// Layer Justification
	UFUNCTION(BlueprintCallable, Category = Justification)
	void UpdateDefaultJustificationZ(float newValue);

	UFUNCTION(BlueprintCallable, Category = Justification)
	void UpdateDefaultJustificationXY(float newValue);

	UFUNCTION(BlueprintCallable, Category = Justification)
	float GetDefaultJustificationZ() const;

	UFUNCTION(BlueprintCallable, Category = Justification)
	float GetDefaultJustificationXY() const;

	UFUNCTION(BlueprintCallable, Category = Justification)
	bool GetMetaPlaneHostedObjJustificationValue(AActor* actor, float& value);

	UFUNCTION(BlueprintCallable, Category = Justification)
	void SetMetaPlaneHostedObjJustificationValue(float newValue, AActor* actor);

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

	UFUNCTION(BlueprintPure)
	bool IsCraftingWidgetActive() const;

	UFUNCTION(BlueprintPure)
	UModumateCraftingWidget_CPP* GetCraftingWidget() const { return CraftingWidget; }

	UFUNCTION(BlueprintPure)
	UModumateDrawingSetWidget_CPP* GetDrawingSetWidget() const { return DrawingSetWidget; }

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

	// Override default hole vertices for furnitures by user
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	bool bOverrideDefaultHoleVertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	float MaxRaycastDist;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Snap)
	TArray<FTransform> UserSnapPoints;

	// Hole Vertices use for override
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	TArray<FVector> HoleVerticesOverride;

	UFUNCTION(BlueprintCallable, Category = Modeling)
	void GroupSelected(bool makeGroup);

	UFUNCTION(BlueprintCallable, Category = Mouse)
	FVector GetZoomLocation(float dir); //grab target for zoom animation

	UFUNCTION(BlueprintCallable, Category = Camera)
	bool ZoomToProjectExtents();

	UFUNCTION(BlueprintCallable, Category = Camera)
	bool ZoomToSelection();

	// Calculate the location needed to view the boundary set by TargetSphere
	FVector CalculateViewLocationForSphere(const FSphere &TargetSphere, const FVector &ViewVector, float AspectRatio, float FOV);

	// Set the scale of window preview model
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	FVector WindowPreviewScale = FVector(1.f, 1.f, 1.f);

	// Hole Vertices use for override
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	TArray<FVector> HoleVerticesDoorOverride;

	// Set the scale of door preview model
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	FVector DoorPreviewScale = FVector(1.f, 1.213f, 1.0668f);

	// Draw "Total Line" during interaction with adjustment handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	bool EnableDrawTotalLine = true;

	// Draw "Delta Line" during interaction with adjustment handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	bool EnableDrawDeltaLine = true;

	// Let user input override vertical dimension string of portal obj
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	bool EnablePortalVerticalInput = true;

	// Let user input override horizontal dimension string of portal obj
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	bool EnablePortalHorizontalInput = true;

	// The actor selected by editing text box widget.
	// When portal obj is selected, it shows both horizontal and vertical dim string Editable Text Box
	// This saves which actor should the Editable Text Box be editing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modeling)
	AActor* DimStringWidgetSelectedObject;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	UHUDDrawWidget_CPP* HUDDrawWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	bool RenderPreviewAssembly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	UTexture2D* StaticCamTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Drafting)
	TSubclassOf<UDraftingPreviewWidget_CPP> PreviewWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Drafting)
	TSubclassOf<UModumateCraftingWidget_CPP> CraftingWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Drafting)
	TSubclassOf<UModumateDrawingSetWidget_CPP> DrawingSetWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Category = Snap, ToolTip = "The distance bias at which we will prefer hits against virtual objects (snaps, MOIs that are just points/lines) to direct hits."))
	float VirtualHitBias = 0.1f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringGroupID_Default = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringGroupID_PlayerController = TEXT("PlayerController");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringUniqueID_Delta = TEXT("Delta");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringUniqueID_Total = TEXT("Total");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringUniqueID_ControllerLine = TEXT("ControllerLine");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringUniqueID_PortalWidth = TEXT("PortalWidth");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringUniqueID_PortalHeight = TEXT("PortalHeight");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringUniqueID_SideEdge0 = TEXT("SideEdge0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DimensionString)
	FName DimensionStringUniqueID_SideEdge1 = TEXT("SideEdge1");

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
	AAdjustmentHandleActor_CPP *InteractionHandle;

	UPROPERTY()
	AAdjustmentHandleActor_CPP *HoverHandle;

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
	bool HandleToolInputText(FString InputText);

	UFUNCTION(BlueprintPure, Category = "Tools")
	bool CanCurrentHandleShowRawInput();

	// Input handling
	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnMButtonDown();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnMButtonUp();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnLButtonDown();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnLButtonUp();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnRButtonDown();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnRButtonUp();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	void OnMouseMove();

	UFUNCTION(BlueprintCallable, Category = Mouse)
	bool IsHandleValid();

	bool IsShiftDown() const;
	bool IsControlDown() const;

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	bool HandleSpacebar();

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	bool HandleEscapeKey();

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	bool HandleBareControlKey(bool pressed);

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	bool HandleTabKeyForDimensionString();

	// Set dim. string tab state back to default
	UFUNCTION(BlueprintCallable, Category = Keyboard)
	void ResetDimensionStringTabState();

	// Set dim. string tab state in portal back to default
	UFUNCTION(BlueprintCallable, Category = Keyboard)
	void ResetDimensionStringTabStatePortal();

	UFUNCTION(BlueprintPure, Category = Keyboard)
	FString GetTextBoxUserInput() { return TextBoxUserInput; }

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	void SetTextBoxUserInput(const FString Input) { TextBoxUserInput = Input; }

	UFUNCTION(BlueprintPure, Category = Keyboard)
	bool HasPendingTextBoxUserInput() { return !TextBoxUserInput.IsEmpty(); }

	bool HandleInputNumber(double inputNumber);

	UFUNCTION(BlueprintImplementableEvent, Category = Draw)
	void DrawRotateToolDegree(const float Degree, const FVector Location, const AActor* DrawLine1, const AActor* DrawLine2);

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

	UFUNCTION(BlueprintCallable, Category = Persistence)
	bool ExportPDF();

	bool OnSavePDF();
	bool OnCancelPDF();

	UFUNCTION(BlueprintCallable, Category = Persistence)
	void TrySavePDF();

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
};
