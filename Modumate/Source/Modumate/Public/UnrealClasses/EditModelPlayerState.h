// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/ModumateObjectEnums.h"
#include "DocumentManagement/DocumentDelta.h"
#include "Objects/ModumateObjectInstance.h"
#include "Online/ModumateAccountManager.h"
#include "GameFramework/PlayerState.h"
#include "ModumateCore/ModumateDimensionString.h"
#include "Objects/CameraView.h"
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

USTRUCT()
struct MODUMATE_API FWebPlayerDetails
{
	GENERATED_BODY()

	UPROPERTY()
	FString id;
	
	UPROPERTY()
	FString color;
};

USTRUCT()
struct MODUMATE_API FWebEditModelPlayerState
{
	GENERATED_BODY()

	//TODO: extend for all player state settings the web interface needs
	UPROPERTY()
	TArray<FWebMOI> selectedObjects;

	UPROPERTY()
	TArray<int32> hiddenObjects;

	UPROPERTY()
	FString tool;

	UPROPERTY()
	FString toolPreset;

	UPROPERTY()
	FString toolMode;

	UPROPERTY()
	FString viewMode;

	UPROPERTY()
	int32 culledCutplane;

	UPROPERTY()
	FMOICameraViewData camera;

	UPROPERTY()
	FString projectId;

	UPROPERTY()
	float terrainHeight;

	UPROPERTY()
	TArray<FWebPlayerDetails> players;

	UPROPERTY()
	int32 clientIdx = INDEX_NONE;
};

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
	float DefaultTerrainHeight =  12.0f * UModumateDimensionStatics::InchesToCentimeters;
	void TryInitController();
	void BatchRenderLines();
	void UpdateRenderFlags(const TSet<AModumateObjectInstance*>& ChangedObjects);

public:

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowDebugSnaps;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowDocumentDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowGraphDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowVolumeDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowSurfaceDebug;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowSpanDebug;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDevelopDDL2Data;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowMultiplayerDebug;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowDesignOptionDebug;

	bool bDrawingDesignerVisible = false;

	bool bBeganWithController = false;

	FString LastFilePath;

	void SetShowGraphDebug(bool bShow);

	EEditViewModes SelectedViewMode;
	bool bRoomsVisible;
	bool ShowingFileDialog;

	AModumateObjectInstance *HoveredObject;
	AModumateObjectInstance *HoveredGroup = nullptr;
	FMOICameraViewData CachedInputCameraState;

	bool ToWebPlayerState(FWebEditModelPlayerState& OutState) const;
	bool FromWebPlayerState(const FWebEditModelPlayerState& InState);
	bool SendWebPlayerState() const;

	void AddHideObjectsById(const TArray<int32>& IDs, bool bUpdateDesignOptions=true);
	void UnhideAllObjects();
	void UnhideObjectsById(const TArray<int32>& ids, bool bUpdateDesignOptions = true);
	const TSet<int32>& GetHiddenObjectsId() const { return HiddenObjectsID; }
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
	void SetHoveredObject(AModumateObjectInstance* Object);
	void SetHoveredGroup(AModumateObjectInstance* GroupObject);
	void SetObjectSelected(AModumateObjectInstance *ob, bool bSelected, bool bDeselectOthers);
	void SetObjectsSelected(const TSet<AModumateObjectInstance*>& Obs, bool bSelected, bool bDeselectOthers);
	void SetGroupObjectSelected(AModumateObjectInstance* GroupObject, bool bSelected, bool bDeselectOthers);
	void SetGroupObjectsSelected(const TSet<AModumateObjectInstance*>& GroupObjects, bool bSelected, bool bDeselectOthers);
	int32 NumItemsSelected() const { return SelectedObjects.Num() + SelectedGroupObjects.Num(); }

	void FindReachableObjects(TSet<AModumateObjectInstance*> &ReachableObjs) const;
	bool IsObjectReachableInView(AModumateObjectInstance* obj) const;
	bool ValidateSelectionsAndView();
	void SelectAll();
	void SelectInverse();
	void DeselectAll(bool bNotifyWidget = true);
	void SetActorRenderValues(AActor* actor, int32 stencilValue, bool bNeverCull);
	void PostSelectionChanged();
	void PostViewChanged();
	void PostGroupChanged(const TArray<int32>& ChangedGroups);

	void CopySelectedToClipboard(const UModumateDocument &document);
	void Paste(UModumateDocument &document) const;

	void DebugShowWallProfiles(const TArray<AModumateObjectInstance *> &walls);

	void SetDefaultTerrainHeight(float height) { DefaultTerrainHeight = height; };
	float GetDefaultTerrainHeight() const { return DefaultTerrainHeight; }

	UPROPERTY()
	AEditModelPlayerController *EMPlayerController;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UMaterialParameterCollection *MetaPlaneMatParamCollection;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite)
	FLinearColor DefaultPlayerColor;

	TSet<AModumateObjectInstance *> SelectedObjects;
	TSet<AModumateObjectInstance *> LastSelectedObjectSet;

	TSet<AModumateObjectInstance *> HoveredObjectDescendents;
	TSet<AModumateObjectInstance *> LastHoveredObjectSet;

	TSet<AModumateObjectInstance *> ErrorObjects;
	TSet<AModumateObjectInstance *> LastErrorObjectSet;

	TSet<AModumateObjectInstance *> LastReachableObjectSet;

	TSet<AModumateObjectInstance*> SelectedGroupObjects;

	bool ShowHoverEffects;

	TMap<int32, TSet<FName>> ObjectErrorMap;

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

	int32 RayTracingQuality = 0;
	int32 RayTracingExposure = 0;
	bool bShowLights = false;

protected:
	FProjectPermissionsEvent ProjectPermChangedEvent;

	void UpdateOtherClientCameraAndCursor();
	static const FString ViewOnlyArg;
	static FProjectPermissions ParseProjectPermissions(const TArray<FString>& Permissions);

	TArray<FStructurePoint> TempObjectStructurePoints, CurSelectionStructurePoints;
	TArray<FStructureLine> TempObjectStructureLines, CurSelectionStructureLines;
	TSet<AModumateObjectInstance *> CurViewGroupObjects;

	TSet<int32> HiddenObjectsID;

	enum class EStencilFlags;

private:
	static const FString DefaultEnvDateTime;

	void RenderSelectedAlignmentTargetLines(UModumateDocument& Document, const FMOIAlignment& SelectedMOIAlignment);
};
