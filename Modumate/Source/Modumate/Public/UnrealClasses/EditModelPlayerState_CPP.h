// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "EditModelPlayerHUD_CPP.h"
#include "HUDDrawWidget_CPP.h"
#include "ModumateDimensionString.h"
#include "ModumateObjectEnums.h"
#include "ModumateObjectInstance.h"
#include "ModumateSnappedCursor.h"

#include "EditModelPlayerState_CPP.generated.h"

class AAdjustmentHandleActor_CPP;

namespace Modumate
{
	class FModumateDocument;
	class IModumateEditorTool;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionObjectChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPostOnNewModel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpdateCutPlanes);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpdateScopeBoxes);

/**
 *
 */
UCLASS(Config=Game)
class MODUMATE_API AEditModelPlayerState_CPP : public APlayerState
{
	GENERATED_BODY()
	AEditModelPlayerState_CPP();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	friend class AEditModelPlayerController_CPP;
	TArray<FMOIDataRecord> ClipboardEntries;

protected:
	void BatchRenderLines();

public:

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool ShowDebugSnaps;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool ShowDocumentDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowGraphDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowVolumeDebug;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDevelopDDL2Data;

	FString LastFilePath;

	void SetShowGraphDebug(bool bShow);

	EEditViewModes SelectedViewMode;
	bool ShowingFileDialog;
	TMap<EToolMode, Modumate::IModumateEditorTool *> ModeToTool;
	TMap<Modumate::IModumateEditorTool *, EToolMode> ToolToMode;
	Modumate::IModumateEditorTool *CurrentTool;
	EToolMode ReturnToolMode;
	float CurrentToolUseDuration;
	EEditViewModes PreviousModeFromToggleRoomView;

	Modumate::FModumateObjectInstance *HoveredObject;
	AAdjustmentHandleActor_CPP *HoverHandle;

	bool DebugMouseHits;

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool ToolIsInUse() const;

	UFUNCTION(BlueprintCallable, Category = "Game")
	AEditModelGameMode_CPP *GetEditModelGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	TArray<FAffordanceLine> AffordanceLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	TArray<FModelDimensionString> DimensionStrings;

	// Current index of dimension string within the group ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 CurrentDimensionStringGroupIndex = 0;

	// Max index supported by the current dim string group. Used for TAB switching between dim string of the same groupID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 CurrentDimensionStringGroupIndexMax = 0;

	// The UniqueID of dim string that is expecting user input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FName CurrentDimensionStringWithInputUniqueID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FSnappedCursor SnappedCursor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tools")
	bool bShowSnappedCursor;

	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FOnSelectionChanged OnSelectionChanged;

	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FOnSelectionObjectChanged OnSelectionObjectChanged;

	// Blueprints will use this to handle post load events
	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FPostOnNewModel PostOnNewModel;

	// Blueprints will use this to handle updates to cut plane browser
	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FOnUpdateCutPlanes OnUpdateCutPlanes;

	// Blueprints will use this to handle updates to scope box browser
	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FOnUpdateScopeBoxes OnUpdateScopeBoxes;

	bool GetSnapCursorDeltaFromRay(const FVector& RayOrigin, const FVector& RayDir, FVector& OutPosition) const;

	void SetToolModeDirect(EToolMode tm);

	UFUNCTION(BlueprintCallable, Category = "Tools", DisplayName = "Set Tool Mode", meta = (ScriptMethod = "SetToolMode"))
	void SetToolModeCommand(EToolMode tm);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	EToolMode GetToolMode();

	UFUNCTION(BlueprintPure, Category = "Tools")
	EEditViewModes GetSelectedViewMode() { return SelectedViewMode; }

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void AbortUseTool();

	bool SetObjectHeight(Modumate::FModumateObjectInstance *obj, float newHeight, bool bSetBaseElevation, bool bUpdateGeometry);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool SetSelectedHeight(float HeightEntry, bool bIsDelta, bool bUpdateGeometry);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool SetSelectedControlPointsHeight(float newCPHeight, bool bIsDelta, bool bUpdateGeometry);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool GetHasActiveTool();

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void SetAssemblyForActor(AActor *actor, const FShoppingItem &assembly);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void SetToolAxisConstraint(EAxisConstraint AxisConstraint);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void SetToolCreateObjectMode(EToolCreateObjectMode CreateObjectMode);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void ToggleRoomViewMode();

	UPROPERTY()
	AAdjustmentHandleActor_CPP *InteractionHandle;

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool HandleToolInputText(FString inputText);

	UFUNCTION(BlueprintPure, Category = "Tools")
	bool CanCurrentHandleShowRawInput();

	void OnLButtonDown();
	void OnLButtonUp();


	void OnNewModel();

	void SetShowHoverEffects(bool showHoverEffects);
	Modumate::FModumateObjectInstance *GetValidHoveredObjectInView(Modumate::FModumateObjectInstance *hoverTarget) const;
	void SetHoveredObject(Modumate::FModumateObjectInstance *ob);
	void SetObjectSelected(Modumate::FModumateObjectInstance *ob, bool selected);
	void SetViewGroupObject(Modumate::FModumateObjectInstance *ob);
	bool IsObjectInCurViewGroup(Modumate::FModumateObjectInstance *ob) const;
	Modumate::FModumateObjectInstance *FindHighestParentGroupInViewGroup(Modumate::FModumateObjectInstance *ob) const;
	void FindReachableObjects(TSet<Modumate::FModumateObjectInstance*> &reachableObjs) const;
	bool IsObjectReachableInView(Modumate::FModumateObjectInstance* obj) const;
	bool ValidateSelectionsAndView();
	void SelectAll();
	void SelectInverse();
	void DeselectAll();
	void SetActorRenderValues(AActor* actor, int32 stencilValue, bool bNeverCull);
	void PostSelectionOrViewChanged();

	void CopySelectedToClipboard(const Modumate::FModumateDocument &document);
	void Paste(Modumate::FModumateDocument &document) const;

	FModumateObjectAssembly SetTemporaryAssembly(EToolMode mode, const FModumateObjectAssembly &assembly);
	const FModumateObjectAssembly *GetTemporaryAssembly(EToolMode mode) const;

	void DebugShowWallProfiles(const TArray<Modumate::FModumateObjectInstance *> &walls);

	UPROPERTY()
	AEditModelPlayerController_CPP *EMPlayerController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UMaterialParameterCollection *MetaPlaneMatParamCollection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor MetaPlaneHorizontalColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor MetaPlaneVerticalColor;

	UPROPERTY(EditAnywhere, BluePrintReadWrite)
	FColor MetaPlaneDisconnectedColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor MetaPlaneAskewColor;

	UFUNCTION()
	FColor GetMetaPlaneColor(FVector Normal, bool bSwapVerticalHorizontal = false, bool bConnected = true);

	// The MOI of type Group that is the current selection view scope.
	// Clicking outside of it or escaping will go to the next highest group in the hierarchy,
	// so normally only its children can be selected directly with the tool.
	// Selecting one of its children that is also a group will deepen the view to that new group.
	Modumate::FModumateObjectInstance *ViewGroupObject = nullptr;

	TArray<Modumate::FModumateObjectInstance *> SelectedObjects;
	TSet<Modumate::FModumateObjectInstance *> LastSelectedObjectSet;
	TSet<Modumate::FModumateObjectInstance *> LastViewGroupDescendentsSet;
	TSet<Modumate::FModumateObjectInstance *> LastHoveredObjectSet;
	TSet<Modumate::FModumateObjectInstance *> LastErrorObjectSet;
	TSet<Modumate::FModumateObjectInstance *> LastReachableObjectSet;
	bool ShowHoverEffects;

	TMap<int32, TSet<FName>> ObjectErrorMap;

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void GetSelectorModumateObjects(TArray<AActor*>& ModumateObjects);

	UFUNCTION(BlueprintPure, Category = "Tools")
	AActor* GetViewGroupObject() const;

	UFUNCTION(BlueprintPure, Category = "Tools")
	AActor* GetHoveredObject() const;

	// Used to refresh FOV value in UI
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnCameraFOVUpdate(float UpdatedFOV);

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void RefreshActiveAssembly();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tools")
	void OnHeightUpdate();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tools")
	void OnWallUpdate();

	UFUNCTION(BlueprintImplementableEvent, Category = "Tools")
	void RefreshCreateSimilarActor(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool SetErrorForObject(int32 ObjectID, FName ErrorTag, bool bIsError);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool ClearErrorsForObject(int32 ObjectID);

	UFUNCTION(BlueprintPure, Category = "Tools")
	bool DoesObjectHaveError(int32 ObjectID, FName ErrorTag) const;

	UFUNCTION(BlueprintPure, Category = "Tools")
	bool DoesObjectHaveAnyError(int32 ObjectID) const;

	UFUNCTION(BlueprintPure, Category = "Tools")
	bool AnySelectedPlacementErrors() const;

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void ClearSelectedPlacementErrorsAndHandles();

	UFUNCTION(BlueprintPure, Category = "Tools")
	int32 GetViewGroupObjectID() const { return ViewGroupObject ? ViewGroupObject->ID : 0; }

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	void SetAssemblyForToolMode(EToolMode mode, const FShoppingItem &item);

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	const FShoppingItem &GetAssemblyForToolMode(EToolMode mode);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	TArray<FModelDimensionString> GetDimensionStringFromGroupID(const FName &groupID, int32 &maxIndexInGroup);

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	void AddDimensionStringsToHUDDrawWidget();

	UFUNCTION(BlueprintCallable, Category = Keyboard)
	TArray<FModumateLines> ConvertDimensionStringsToModumateLines(FModelDimensionString dimString);

	UFUNCTION(BlueprintCallable)
	void SetEditViewModeDirect(EEditViewModes NewEditViewMode, bool bForceUpdate = false);

	UFUNCTION(BlueprintCallable)
	void SetEditViewModeCommand(EEditViewModes NewEditViewMode);

	UFUNCTION()
	void UpdateObjectVisibilityAndCollision();

	UFUNCTION()
	bool IsObjectTypeEnabledByViewMode(EObjectType ObjectType) const;

protected:
	TArray<Modumate::FStructurePoint> TempObjectStructurePoints, CurSelectionStructurePoints;
	TArray<Modumate::FStructureLine> TempObjectStructureLines, CurSelectionStructureLines;
	TSet<Modumate::FModumateObjectInstance *> CurViewGroupObjects;
	TMap<EToolMode, FModumateObjectAssembly> TemporaryAssemblies;
};
