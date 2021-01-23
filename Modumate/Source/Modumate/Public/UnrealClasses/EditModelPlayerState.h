// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "Objects/ModumateObjectInstance.h"
#include "GameFramework/PlayerState.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"
#include "UObject/ScriptInterface.h"

#include "EditModelPlayerState.generated.h"

class MODUMATE_API UModumateDocument;
class MODUMATE_API AGraphDimensionActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionOrViewChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPostOnNewModel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpdateScopeBoxes);

/**
 *
 */
UCLASS(Config=Game)
class MODUMATE_API AEditModelPlayerState : public APlayerState
{
	GENERATED_BODY()
	AEditModelPlayerState();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	friend class AEditModelPlayerController;

protected:
	void BatchRenderLines();
	void UpdateRenderFlags(const TSet<AModumateObjectInstance*>& ChangedObjects);

public:

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool ShowDebugSnaps;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool ShowDocumentDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowGraphDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowVolumeDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowSurfaceDebug;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDevelopDDL2Data;

	FString LastFilePath;

	void SetShowGraphDebug(bool bShow);

	EEditViewModes SelectedViewMode;
	bool bRoomsVisible;
	bool ShowingFileDialog;

	AModumateObjectInstance *HoveredObject;

	bool DebugMouseHits;

	UFUNCTION(BlueprintCallable, Category = "Game")
	AEditModelGameMode *GetEditModelGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	TArray<FAffordanceLine> AffordanceLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FSnappedCursor SnappedCursor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tools")
	bool bShowSnappedCursor;

	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FOnSelectionOrViewChanged OnSelectionOrViewChanged;

	// Blueprints will use this to handle post load events
	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FPostOnNewModel PostOnNewModel;

	// Blueprints will use this to handle updates to scope box browser
	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Tools")
	FOnUpdateScopeBoxes OnUpdateScopeBoxes;

	bool GetSnapCursorDeltaFromRay(const FVector& RayOrigin, const FVector& RayDir, FVector& OutPosition) const;

	UFUNCTION(BlueprintPure, Category = "Tools")
	EEditViewModes GetViewMode() { return SelectedViewMode; }

	UFUNCTION(BlueprintCallable, Category = "Tools")
	void ToggleRoomViewMode();


	void OnNewModel();

	void SetShowHoverEffects(bool showHoverEffects);
	AModumateObjectInstance *GetValidHoveredObjectInView(AModumateObjectInstance *hoverTarget) const;
	void SetHoveredObject(AModumateObjectInstance *ob);
	void SetObjectSelected(AModumateObjectInstance *ob, bool selected);
	void SetViewGroupObject(AModumateObjectInstance *ob);
	bool IsObjectInCurViewGroup(AModumateObjectInstance *ob) const;
	AModumateObjectInstance *FindHighestParentGroupInViewGroup(AModumateObjectInstance *ob) const;
	void FindReachableObjects(TSet<AModumateObjectInstance*> &reachableObjs) const;
	bool IsObjectReachableInView(AModumateObjectInstance* obj) const;
	bool ValidateSelectionsAndView();
	void SelectAll();
	void SelectInverse();
	void DeselectAll();
	void SetActorRenderValues(AActor* actor, int32 stencilValue, bool bNeverCull);
	void PostSelectionChanged();
	void PostViewChanged();

	void CopySelectedToClipboard(const UModumateDocument &document);
	void Paste(UModumateDocument &document) const;

	void DebugShowWallProfiles(const TArray<AModumateObjectInstance *> &walls);

	UPROPERTY()
	AEditModelPlayerController *EMPlayerController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UMaterialParameterCollection *MetaPlaneMatParamCollection;

	// The MOI of type Group that is the current selection view scope.
	// Clicking outside of it or escaping will go to the next highest group in the hierarchy,
	// so normally only its children can be selected directly with the tool.
	// Selecting one of its children that is also a group will deepen the view to that new group.
	AModumateObjectInstance *ViewGroupObject = nullptr;

	TSet<AModumateObjectInstance *> SelectedObjects;
	TSet<AModumateObjectInstance *> LastSelectedObjectSet;

	TSet<AModumateObjectInstance *> ViewGroupDescendents;
	TSet<AModumateObjectInstance *> LastViewGroupDescendentsSet;

	TSet<AModumateObjectInstance *> HoveredObjectDescendents;
	TSet<AModumateObjectInstance *> LastHoveredObjectSet;

	TSet<AModumateObjectInstance *> ErrorObjects;
	TSet<AModumateObjectInstance *> LastErrorObjectSet;

	TSet<AModumateObjectInstance *> LastReachableObjectSet;
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

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool SetErrorForObject(int32 ObjectID, FName ErrorTag, bool bIsError);

	UFUNCTION(BlueprintCallable, Category = "Tools")
	bool ClearErrorsForObject(int32 ObjectID);

	UFUNCTION(BlueprintPure, Category = "Tools")
	bool DoesObjectHaveError(int32 ObjectID, FName ErrorTag) const;

	UFUNCTION(BlueprintPure, Category = "Tools")
	bool DoesObjectHaveAnyError(int32 ObjectID) const;

	UFUNCTION(BlueprintPure, Category = "Tools")
	int32 GetViewGroupObjectID() const { return ViewGroupObject ? ViewGroupObject->ID : 0; }

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	void SetAssemblyForToolMode(EToolMode Mode, const FGuid &AssemblyGUID);

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	FGuid GetAssemblyForToolMode(EToolMode mode);

	UFUNCTION()
	bool SetViewMode(EEditViewModes NewEditViewMode, bool bForceUpdate = false);

	UFUNCTION()
	bool ChangeViewMode(int32 IndexDelta);

	UFUNCTION()
	void UpdateObjectVisibilityAndCollision();

	UFUNCTION()
	bool IsObjectTypeEnabledByViewMode(EObjectType ObjectType) const;

protected:
	TArray<FStructurePoint> TempObjectStructurePoints, CurSelectionStructurePoints;
	TArray<FStructureLine> TempObjectStructureLines, CurSelectionStructureLines;
	TSet<AModumateObjectInstance *> CurViewGroupObjects;
};
