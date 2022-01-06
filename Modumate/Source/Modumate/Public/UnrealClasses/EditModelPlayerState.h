// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Database/ModumateObjectEnums.h"
#include "DocumentManagement/DocumentDelta.h"
#include "Objects/ModumateObjectInstance.h"
#include "Online/ModumateAccountManager.h"
#include "GameFramework/PlayerState.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/HUDDrawWidget.h"
#include "UObject/ScriptInterface.h"
#include "ToolsAndAdjustments/Common/ModumateInterpBuffer.h"

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool BeginWithController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Destroyed() override;

	virtual void ClientInitialize(class AController* C) override;

	friend class AEditModelPlayerController;

protected:
	void TryInitController();
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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowMultiplayerDebug;

	bool bBeganWithController = false;

	FString LastFilePath;

	void SetShowGraphDebug(bool bShow);

	EEditViewModes SelectedViewMode;
	bool bRoomsVisible;
	bool ShowingFileDialog;

	AModumateObjectInstance *HoveredObject;

	bool DebugMouseHits;

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

	UFUNCTION()
	void SetObjectIDSelected(int32 ObjID, bool bSelected);

	void SetShowHoverEffects(bool showHoverEffects);
	AModumateObjectInstance *GetValidHoveredObjectInView(AModumateObjectInstance *hoverTarget) const;
	void SetHoveredObject(AModumateObjectInstance *ob);
	void SetObjectSelected(AModumateObjectInstance *ob, bool bSelected, bool bDeselectOthers);
	void SetObjectsSelected(TSet<AModumateObjectInstance*>& Obs, bool bSelected, bool bDeselectOthers);
	void SetGroupObjectSelected(AModumateObjectInstance* GroupObject, bool bSelected, bool bDeselectOthers);

	void SetViewGroupObject(AModumateObjectInstance *ob);
	bool IsObjectInCurViewGroup(AModumateObjectInstance *ob) const;
	AModumateObjectInstance *FindHighestParentGroupInViewGroup(AModumateObjectInstance *ob) const;
	void FindReachableObjects(TSet<AModumateObjectInstance*> &reachableObjs) const;
	bool IsObjectReachableInView(AModumateObjectInstance* obj) const;
	bool ValidateSelectionsAndView();
	void SelectAll();
	void SelectInverse();
	void DeselectAll(bool bNotifyWidget = true);
	void SetActorRenderValues(AActor* actor, int32 stencilValue, bool bNeverCull);
	void PostSelectionChanged();
	void PostViewChanged();
	void PostGroupChanged(const TArray<int32> ChangedGroups);

	void CopySelectedToClipboard(const UModumateDocument &document);
	void Paste(UModumateDocument &document) const;

	void DebugShowWallProfiles(const TArray<AModumateObjectInstance *> &walls);

	UPROPERTY()
	AEditModelPlayerController *EMPlayerController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UMaterialParameterCollection *MetaPlaneMatParamCollection;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite)
	FLinearColor DefaultPlayerColor;

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

	TSet<AModumateObjectInstance*> SelectedGroupObjects;

	bool ShowHoverEffects;

	TMap<int32, TSet<FName>> ObjectErrorMap;

	UFUNCTION(BlueprintPure, Category = "Tools")
	AActor* GetViewGroupObject() const;

	UFUNCTION(BlueprintPure, Category = "Tools")
	AActor* GetHoveredObject() const;

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

	UFUNCTION()
	void ClearConflictingRedoBuffer(const FDeltasRecord& Deltas);

	UFUNCTION()
	FLinearColor GetClientColor() const;

	UFUNCTION()
	void UpdateAllUsersList();

	// Networking/replication-related functions and properties

	UFUNCTION(Server, Reliable)
	void SetUserInfo(const FModumateUserInfo& UserInfo, int32 ClientIdx);

	UFUNCTION(Server, Reliable)
	void SendClientDeltas(FDeltasRecord Deltas);

	UFUNCTION(Server, Reliable)
	void TryUndo();

	UFUNCTION(Server, Reliable)
	void TryRedo();

	UFUNCTION(Client, Reliable)
	void SendInitialState(const FString& ProjectID, int32 UserIdx, uint32 CurDocHash);

	UFUNCTION(Server, Reliable)
	void OnDownloadedDocument(uint32 DownloadedDocHash);

	UFUNCTION(Client, Reliable)
	void RollBackUnverifiedDeltas(uint32 VerifiedDocHash);

	UFUNCTION(Server, Unreliable)
	void UpdateCameraUnreliable(const FTransform& NewTransform);

	UFUNCTION(Server, Reliable)
	void UpdateCameraReliable(const FTransform& NewTransform);

	UFUNCTION(Server, Unreliable)
	void UpdateCursorLocationUnreliable(const FVector& NewLocation);

	UFUNCTION(Server, Reliable)
	void RequestUpload();

	UFUNCTION(Server, Reliable)
	void UploadProjectThumbnail(const FString& Base64String);

	UFUNCTION(Server, Reliable)
	void SetupPermissions();
	bool RequestPermissions(const FString& UserID, const FString& ProjecID, const TFunction<void(const FString&, const FString&, const FProjectPermissions&)>& Callback);

	UFUNCTION()
	void OnRep_CamTransform();

	UFUNCTION()
	void OnRep_CursorLocation();

	UFUNCTION()
	void OnRep_UserInfo();

	UFUNCTION()
	void OnRep_UserPermissions();

	UPROPERTY(ReplicatedUsing=OnRep_CamTransform)
	FTransform ReplicatedCamTransform;

	UPROPERTY(ReplicatedUsing = OnRep_CursorLocation)
	FVector ReplicatedCursorLocation;

	UPROPERTY(ReplicatedUsing=OnRep_UserInfo)
	FModumateUserInfo ReplicatedUserInfo;

	UPROPERTY(ReplicatedUsing=OnRep_UserPermissions)
	FProjectPermissions ReplicatedProjectPermissions;

	UPROPERTY(Replicated)
	int32 MultiplayerClientIdx = INDEX_NONE;

	// The FDeltasRecords that this user has undone (either directly, or because they were implicated in this user's intended undo)
	UPROPERTY()
	TArray<FDeltasRecord> UndoneDeltasRecords;

	FString CurProjectID;
	bool bPendingClientDownload = false;
	uint32 ExpectedDownloadDocHash = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	FModumateInterpBuffer CamReplicationTransformBuffer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multiplayer")
	FModumateInterpBuffer CursorReplicationTransformBuffer;

	FVector InterpReplicatedCursorLocation;

	DECLARE_EVENT(AEditModelPlayerState, FProjectPermissionsEvent)
	FProjectPermissionsEvent& OnProjectPermissionsChanged() { return ProjectPermChangedEvent; }

protected:
	FProjectPermissionsEvent ProjectPermChangedEvent;

	void UpdateOtherClientCameraAndCursor();
	static const FString ViewOnlyArg;
	static FProjectPermissions ParseProjectPermissions(const TArray<FString>& Permissions);

	TArray<FStructurePoint> TempObjectStructurePoints, CurSelectionStructurePoints;
	TArray<FStructureLine> TempObjectStructureLines, CurSelectionStructureLines;
	TSet<AModumateObjectInstance *> CurViewGroupObjects;

	enum class EStencilFlags;
};
