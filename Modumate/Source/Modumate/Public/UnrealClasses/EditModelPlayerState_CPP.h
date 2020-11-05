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

#include "EditModelPlayerState_CPP.generated.h"

class MODUMATE_API FModumateDocument;
class MODUMATE_API AGraphDimensionActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionOrViewChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPostOnNewModel);
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
	TArray<FMOIDataRecord_DEPRECATED> ClipboardEntries;

protected:
	void BatchRenderLines();
	void UpdateRenderFlags(const TSet<FModumateObjectInstance*>& ChangedObjects);

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

	FModumateObjectInstance *HoveredObject;

	bool DebugMouseHits;

	UFUNCTION(BlueprintCallable, Category = "Game")
	AEditModelGameMode_CPP *GetEditModelGameMode();

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
	FModumateObjectInstance *GetValidHoveredObjectInView(FModumateObjectInstance *hoverTarget) const;
	void SetHoveredObject(FModumateObjectInstance *ob);
	void SetObjectSelected(FModumateObjectInstance *ob, bool selected);
	void SetViewGroupObject(FModumateObjectInstance *ob);
	bool IsObjectInCurViewGroup(FModumateObjectInstance *ob) const;
	FModumateObjectInstance *FindHighestParentGroupInViewGroup(FModumateObjectInstance *ob) const;
	void FindReachableObjects(TSet<FModumateObjectInstance*> &reachableObjs) const;
	bool IsObjectReachableInView(FModumateObjectInstance* obj) const;
	bool ValidateSelectionsAndView();
	void SelectAll();
	void SelectInverse();
	void DeselectAll();
	void SetActorRenderValues(AActor* actor, int32 stencilValue, bool bNeverCull);
	void PostSelectionChanged();
	void PostViewChanged();

	void CopySelectedToClipboard(const FModumateDocument &document);
	void Paste(FModumateDocument &document) const;

	void DebugShowWallProfiles(const TArray<FModumateObjectInstance *> &walls);

	UPROPERTY()
	AEditModelPlayerController_CPP *EMPlayerController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UMaterialParameterCollection *MetaPlaneMatParamCollection;

	// The MOI of type Group that is the current selection view scope.
	// Clicking outside of it or escaping will go to the next highest group in the hierarchy,
	// so normally only its children can be selected directly with the tool.
	// Selecting one of its children that is also a group will deepen the view to that new group.
	FModumateObjectInstance *ViewGroupObject = nullptr;

	TSet<FModumateObjectInstance *> SelectedObjects;
	TSet<FModumateObjectInstance *> LastSelectedObjectSet;

	TSet<FModumateObjectInstance *> ViewGroupDescendents;
	TSet<FModumateObjectInstance *> LastViewGroupDescendentsSet;

	TSet<FModumateObjectInstance *> HoveredObjectDescendents;
	TSet<FModumateObjectInstance *> LastHoveredObjectSet;

	TSet<FModumateObjectInstance *> ErrorObjects;
	TSet<FModumateObjectInstance *> LastErrorObjectSet;

	TSet<FModumateObjectInstance *> LastReachableObjectSet;
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
	void SetAssemblyForToolMode(EToolMode Mode, const FBIMKey &AssemblyKey);

	UFUNCTION(BlueprintCallable, Category = "Shopping")
	FBIMKey GetAssemblyForToolMode(EToolMode mode);

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
	TSet<FModumateObjectInstance *> CurViewGroupObjects;
};
