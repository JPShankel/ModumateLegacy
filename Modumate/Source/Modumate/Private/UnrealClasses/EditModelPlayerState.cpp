// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelPlayerState.h"

#include "Algo/Transform.h"
#include "Database/ModumateObjectDatabase.h"
#include "Database/ModumateObjectEnums.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Net/UnrealNetwork.h"
#include "Online/ModumateAccountManager.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Online/ModumateCloudConnection.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/DimensionManager.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Online/ModumateClientIcon.h"
#include "UI/Online/OnlineUserName.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerPawn.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/ThumbnailCacheManager.h"

const FString AEditModelPlayerState::ViewOnlyArg(TEXT("ModViewOnly"));

enum class AEditModelPlayerState::EStencilFlags { Selected = 0x1, LegacyGroup = 0x2, Hovered = 0x4, Error = 0x8,  InactiveGroup = 0x10};

AEditModelPlayerState::AEditModelPlayerState()
	: bShowDebugSnaps(false)
	, bShowDocumentDebug(false)
	, bShowGraphDebug(false)
	, bShowVolumeDebug(false)
	, bShowSurfaceDebug(false)
	, bDevelopDDL2Data(false)
#if UE_BUILD_SHIPPING
	, bShowMultiplayerDebug(false)
#else	// TODO: remove multiplayer debugging being the default after we're more stable
	, bShowMultiplayerDebug(true)
#endif
	, SelectedViewMode(EEditViewModes::AllObjects)
	, ShowingFileDialog(false)
	, HoveredObject(nullptr)
	, DebugMouseHits(false)
	, bShowSnappedCursor(true)
	, DefaultPlayerColor(0.011612f, 0.346704f, 1.0f, 1.0f)	// "Modumate Blue"
	, ShowHoverEffects(false)
	, CamReplicationTransformBuffer(6, 0.25f)
	, CursorReplicationTransformBuffer(3, 0.125f)
{
	//DimensionString.Visible = false;
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::AEditModelPlayerState"));

	PrimaryActorTick.bCanEverTick = true;

	// Update at 15fps, rather than 1fps by default, because we are synchronizing very few properties and this is where custom camera movement is replicated.
	NetUpdateFrequency = 15;
}

void AEditModelPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEditModelPlayerState, ReplicatedCamTransform);
	DOREPLIFETIME(AEditModelPlayerState, ReplicatedUserInfo);
	DOREPLIFETIME(AEditModelPlayerState, MultiplayerClientIdx);
	DOREPLIFETIME(AEditModelPlayerState, ReplicatedCursorLocation);
	DOREPLIFETIME(AEditModelPlayerState, ReplicatedProjectPermissions);
}

bool AEditModelPlayerState::BeginWithController()
{
	if (bBeganWithController || (EMPlayerController == nullptr))
	{
		return false;
	}

	PostSelectionChanged();
	PostViewChanged();
	SetViewMode(EEditViewModes::AllObjects, true);

	bBeganWithController = true;

	// Also, if we're a multiplayer client, then broadcast information to other clients
	ULocalPlayer* localPlayer = EMPlayerController ? EMPlayerController->GetLocalPlayer() : nullptr;
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto accountManager = gameInstance ? gameInstance->GetAccountManager() : nullptr;
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;
	if (IsNetMode(NM_Client) && localPlayer && accountManager && cloudConnection && cloudConnection->IsLoggedIn())
	{
		auto userInfo = accountManager->GetUserInfo();
		SetUserInfo(userInfo, MultiplayerClientIdx);
		SetupPermissions(); //On Client, tell server we're ready to setup permissions...
	}

	return true;
}

void AEditModelPlayerState::BeginPlay()
{
	Super::BeginPlay();

	TryInitController();
}

void AEditModelPlayerState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Interpolate the movement of other players' cameras (which can happen without fully initializing with PlayerController)
	UpdateOtherClientCameraAndCursor();

	// Postpone any meaningful work until we have a valid PlayerController
	if (!bBeganWithController || !ensure(EMPlayerController))
	{
		return;
	}

	// Reset the HUD Draw Widget's lines now, so that we know they're all added during this tick function.
	// TODO: should this even be here
	if (EMPlayerController->HUDDrawWidget != nullptr)
	{
		EMPlayerController->HUDDrawWidget->LinesToDraw.Reset();
		BatchRenderLines();
	}
}

void AEditModelPlayerState::Destroyed()
{
	UpdateAllUsersList();

	Super::Destroy();
}

void AEditModelPlayerState::ClientInitialize(class AController* C)
{
	Super::ClientInitialize(C);

	TryInitController();
}

void AEditModelPlayerState::TryInitController()
{
	// This is supposed to be set by the EMPlayerController,
	// but it can be null if we spawn a new PlayerController that still uses this EMPlayerState.
	// (For example, the DebugCameraController, spawned by the CheatManager when pressing the semicolon key in non-shipping builds)
	if (EMPlayerController == nullptr)
	{
		EMPlayerController = GetOwner<AEditModelPlayerController>();
	}

	auto* world = GetWorld();
	auto* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;

	// Multiplayer clients need to wait until they've received a client index, to use for object ID allocation
	if (IsNetMode(NM_Client) && ((MultiplayerClientIdx == INDEX_NONE) || (gameState == nullptr)))
	{
		return;
	}

	// EditModelPlayerState initialization must happen after the EditModelPlayerController,
	// so allow that to either happen now or be deferred.
	if (EMPlayerController && EMPlayerController->HasActorBegunPlay() && !EMPlayerController->bBeganWithPlayerState && gameInstance)
	{
		EMPlayerController->EMPlayerState = this;
		if (EMPlayerController->BeginWithPlayerState() && BeginWithController())
		{
			UE_LOG(LogTemp, Log, TEXT("AEditModelPlayerState initialized state and controller."));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AEditModelPlayerState failed to initialize state and controller!"));
			if (IsNetMode(NM_Client))
			{
				gameInstance->GoToMainMenu(FText::GetEmpty());
			}
		}
	}
}

void AEditModelPlayerState::BatchRenderLines()
{
	CurSelectionStructurePoints.Reset();
	CurSelectionStructureLines.Reset();
	CurViewGroupObjects.Reset();

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;
	FPlane cullingPlane = EMPlayerController->GetCurrentCullingPlane();

	// TODO: remove
	if (ViewGroupObject)
	{
		AModumateObjectInstance *curViewGroupObj = ViewGroupObject;
		while (curViewGroupObj)
		{
			CurViewGroupObjects.Add(curViewGroupObj);
			curViewGroupObj->RouteGetStructuralPointsAndLines(TempObjectStructurePoints, TempObjectStructureLines, false, false, cullingPlane);
			CurSelectionStructurePoints.Append(TempObjectStructurePoints);
			CurSelectionStructureLines.Append(TempObjectStructureLines);
			curViewGroupObj = curViewGroupObj->GetParentObject();
		}
	}

	if ((SelectedObjects.Num() + SelectedGroupObjects.Num() > 0))
	{
		const int32 currentGroup = doc->GetActiveVolumeGraphID();
		TSet<int32> currentSelectedGroups;
		for (auto *selectedObj : SelectedObjects)
		{
			if (selectedObj && selectedObj->ShowStructureOnSelection() && !selectedObj->IsDirty(EObjectDirtyFlags::Structure))
			{
				selectedObj->RouteGetStructuralPointsAndLines(TempObjectStructurePoints, TempObjectStructureLines, false, false, cullingPlane);
				CurSelectionStructurePoints.Append(TempObjectStructurePoints);
				CurSelectionStructureLines.Append(TempObjectStructureLines);
			}
		}

		for(auto* selectedGroupObj: SelectedGroupObjects)
		{
			if (ensure(selectedGroupObj))
			{
				TArray<FStructurePoint> points;
				TArray<FStructureLine> groupBox;

				FModumateLines currentGroupLine;
				currentGroupLine.Thickness = 2.0f;
				currentGroupLine.DashLength = 6.0f;
				currentGroupLine.DashSpacing = 10.0f;
				currentGroupLine.Color = FLinearColor(FColor(0x63, 0xC3, 0xBA));
				selectedGroupObj->RouteGetStructuralPointsAndLines(points, groupBox, false, false, cullingPlane);
				for (const auto& line : groupBox)
				{
					currentGroupLine.Point1 = line.P1;
					currentGroupLine.Point2 = line.P2;
					EMPlayerController->HUDDrawWidget->LinesToDraw.Add(currentGroupLine);
				}
			}
		}
	}

	for (auto &structureLine : CurSelectionStructureLines)
	{
		AModumateObjectInstance *lineObj = doc->GetObjectById(structureLine.ObjID);
		if (lineObj == nullptr)
		{
			continue;
		}

		FModumateLines objectSelectionLine = FModumateLines();
		objectSelectionLine.Point1 = structureLine.P1;
		objectSelectionLine.Point2 = structureLine.P2;
		objectSelectionLine.OwnerActor = lineObj->GetActor();
		objectSelectionLine.Thickness = 2.0f;

		bool inViewGroup = CurViewGroupObjects.Contains(lineObj);
		if ((lineObj->GetObjectType() == EObjectType::OTGroup) || inViewGroup)
		{
			objectSelectionLine.DashLength = 6.0f;
			objectSelectionLine.DashSpacing = 10.0f;
			objectSelectionLine.Color.A = inViewGroup ? 0.2f : 1.0f;
		}
		else
		{
			objectSelectionLine.Color = FLinearColor(FColor(0x63, 0xC3, 0xBA));
		}

		EMPlayerController->HUDDrawWidget->LinesToDraw.Add(MoveTemp(objectSelectionLine));
	}

	if (AffordanceLines.Num() > 0)
	{
		for (const auto &affordanceLine : AffordanceLines)
		{
			FModumateLines lineToDraw = FModumateLines();
			lineToDraw.Point1 = affordanceLine.StartPoint;
			lineToDraw.Point2 = affordanceLine.EndPoint;
			lineToDraw.Color = affordanceLine.Color;
			lineToDraw.DashLength = 0.5f * affordanceLine.Interval;
			lineToDraw.DashSpacing = 2.0f * affordanceLine.Interval;
			lineToDraw.Thickness = affordanceLine.Thickness;
			lineToDraw.Priority = affordanceLine.DrawPriority;

			EMPlayerController->HUDDrawWidget->LinesToDraw.Add(MoveTemp(lineToDraw));
		}

		AffordanceLines.Reset();
	}

	AModumateObjectInstance* outerGroupObject = doc->GetObjectById(doc->GetActiveVolumeGraphID());
	const int32 rootGroup = doc->GetRootVolumeGraphID();
	if (ensure(outerGroupObject))
	{
		FModumateLines outerGroupLine;
		outerGroupLine.Thickness = 1.0f;
		outerGroupLine.DashLength = 3.0f;
		outerGroupLine.DashSpacing = 10.0f;
		outerGroupLine.Color = FLinearColor(0.1f, 0.1f, 0.1f);

		TArray<FStructurePoint> points;
		TArray<FStructureLine> groupBox;

		while (outerGroupObject && outerGroupObject->ID != rootGroup)
		{
			outerGroupObject->RouteGetStructuralPointsAndLines(points, groupBox, false, false, cullingPlane);
			for (const auto& line : groupBox)
			{
				outerGroupLine.Point1 = line.P1;
				outerGroupLine.Point2 = line.P2;
				EMPlayerController->HUDDrawWidget->LinesToDraw.Add(outerGroupLine);
			}
			outerGroupObject = outerGroupObject->GetParentObject();
			outerGroupLine.Color = FLinearColor(FColor(0x87, 0x9a, 0x99));
		}

	}

	// Queue up the lines managed by the PlayerHUD, so that all lines added to the HUD Draw Widget happen here.
	AEditModelPlayerHUD *emPlayerHUD = EMPlayerController->GetEditModelHUD();
	if (emPlayerHUD)
	{
		for (ALineActor *lineActor : emPlayerHUD->All3DLineActors)
		{
			if (lineActor && !lineActor->IsHidden())
			{
				FModumateLines lineData;
				lineData.Point1 = lineActor->Point1;
				lineData.Point2 = lineActor->Point2;
				lineData.Visible = true;
				lineData.Color = FLinearColor(lineActor->Color);
				lineData.Thickness = lineActor->Thickness;

				EMPlayerController->HUDDrawWidget->LinesToDraw.Add(MoveTemp(lineData));
			}
		}
	}

	// TODO: move this code - either be able to ask objects which lines to draw or enable objects to bypass this function
	for (AModumateObjectInstance *cutPlaneObj : doc->GetObjectsOfType(EObjectType::OTCutPlane))
	{
		if (!cutPlaneObj || !cutPlaneObj->IsVisible() || cutPlaneObj->IsDirty(EObjectDirtyFlags::Structure))
		{
			continue;
		}

		cutPlaneObj->AddDraftingLines(EMPlayerController->HUDDrawWidget);
	}

	for (AModumateObjectInstance *scopeBoxObj : doc->GetObjectsOfType(EObjectType::OTScopeBox))
	{
		if (!scopeBoxObj || !scopeBoxObj->IsVisible() || scopeBoxObj->IsDirty(EObjectDirtyFlags::Structure))
		{
			continue;
		}

		scopeBoxObj->AddDraftingLines(EMPlayerController->HUDDrawWidget);
	}
}

void AEditModelPlayerState::UpdateRenderFlags(const TSet<AModumateObjectInstance*>& ChangedObjects)
{
	if (ChangedObjects.Num() > 0)
	{
		OnSelectionOrViewChanged.Broadcast();
	}

	int32 subGroup;  // unused
	bool bIsInGroup;

	for (AModumateObjectInstance *moi : ChangedObjects)
	{
		if (moi && !moi->IsDestroyed())
		{
			auto type = moi->GetObjectType();
			bool bIsGraphType = (UModumateTypeStatics::Graph2DObjectTypeFromObjectType(type) != EGraphObjectType::None) ||
				(UModumateTypeStatics::Graph3DObjectTypeFromObjectType(type) != EGraph3DObjectType::None);
			bool bHovered = (ShowHoverEffects && !bIsGraphType && HoveredObjectDescendents.Contains(moi));
			bool bSelected = SelectedObjects.Contains(moi);
			bool bGroupVisible = UModumateObjectStatics::IsObjectInSubgroup(moi->GetDocument(), moi, MOD_ID_NONE, subGroup, bIsInGroup) || bIsInGroup;

			int32 selectionValue = bSelected ? int(EStencilFlags::Selected) : 0x0;
			int32 hoverValue = (bHovered && !bSelected) ? int(EStencilFlags::Hovered) : 0x0;
			int32 errorValue = ErrorObjects.Contains(moi) ? int(EStencilFlags::Error) : 0x0;
			int32 groupVisibleValue = !bGroupVisible ? int(EStencilFlags::InactiveGroup) : 0x0;
			int32 stencilValue = selectionValue | hoverValue | errorValue | groupVisibleValue;

			SetActorRenderValues(moi->GetActor(), stencilValue, bHovered);
		}
	}
}

void AEditModelPlayerState::SetShowGraphDebug(bool bShow)
{
	if (bShowGraphDebug != bShow)
	{
		bShowGraphDebug = bShow;

		// TODO: replace with non-debug visibility flag for rooms; for now, just piggypack off of the room graph visibility
		AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
		UModumateDocument* doc = gameState->Document;
		TArray<AModumateObjectInstance *> rooms = doc->GetObjectsOfType(EObjectType::OTRoom);
		for (AModumateObjectInstance *room : rooms)
		{
			if (AActor *roomActor = room->GetActor())
			{
				// TODO: Rooms could be refactored to use the UpdateVisibilityAndCollision system,
				// but they're already effectively disabled as long as the 2D room graph is disabled.
				roomActor->SetActorHiddenInGame(!bShowGraphDebug);
				roomActor->SetActorEnableCollision(bShowGraphDebug);
			}
		}
	}
}

void AEditModelPlayerState::ToggleRoomViewMode()
{
	bRoomsVisible = !bRoomsVisible;
	UpdateObjectVisibilityAndCollision();
}

bool AEditModelPlayerState::GetSnapCursorDeltaFromRay(const FVector& RayOrigin, const FVector& RayDir, FVector& OutPosition) const
{
	if (SnappedCursor.Visible)
	{
		OutPosition = SnappedCursor.WorldPosition;
		FPlane RayPlane(RayOrigin, RayDir);

		bool worldAxisSnap = (SnappedCursor.SnapType == ESnapType::CT_WORLDSNAPX) ||
			(SnappedCursor.SnapType == ESnapType::CT_WORLDSNAPY) ||
			(SnappedCursor.SnapType == ESnapType::CT_WORLDSNAPXY);
		bool noHeight = FMath::IsNearlyEqual(SnappedCursor.WorldPosition | RayPlane, RayPlane.W, 0.1f);
		bool notParallel = !FVector::Parallel(SnappedCursor.HitNormal, RayPlane);

		// If we have no snap, a world axis snap, no height difference with the snap,
		// or we snapped against a face whose normal is non-parallel to the cabinet extrusion,
		// then use the virtual vertical axis from the closed loop for the height.
		if (noHeight || worldAxisSnap || (SnappedCursor.SnapType == ESnapType::CT_NOSNAP) ||
			((SnappedCursor.SnapType == ESnapType::CT_FACESELECT) && notParallel))
		{
			FVector CursorWorldPos;
			FVector CursorWorldDir;
			if (EMPlayerController->DeprojectScreenPositionToWorld(SnappedCursor.ScreenPosition.X, SnappedCursor.ScreenPosition.Y, CursorWorldPos, CursorWorldDir))
			{
				FVector PlaneOriginToMouse = RayOrigin - CursorWorldPos;
				FVector PlaneOriginToMouseFlat = FVector::VectorPlaneProject(PlaneOriginToMouse, RayPlane).GetSafeNormal();
				FPlane ProjectionPlane(RayOrigin, PlaneOriginToMouseFlat);
				if (!FVector::Parallel(CursorWorldDir, ProjectionPlane))
				{
					OutPosition = FMath::RayPlaneIntersection(CursorWorldPos, CursorWorldDir, ProjectionPlane);
					return true;
				}
			}
		}
	}

	return false;
}

void AEditModelPlayerState::DebugShowWallProfiles(const TArray<AModumateObjectInstance *> &walls)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::DebugShowWallProfiles"));
}

bool AEditModelPlayerState::ValidateSelectionsAndView()
{
	// TODO: revisit the necessity of this function, because it basically clears out any pointers to destroyed MOIs,
	// which should be more consistently + correctly handled by some different pointer scheme.
	// Most other parts of the codebase that point to FModumateObjectInstances assume that they clear out their pointers
	// as soon as they become invalid (i.e. deleting objects in a parent-child hierarchy).

	bool selectionChanged = false;
	bool viewChanged = false;

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;

	TSet<AModumateObjectInstance*> pendingDeselectObjects;
	for (auto& moi : SelectedObjects)
	{
		if ((moi == nullptr) || moi->IsDestroyed())
		{
			pendingDeselectObjects.Add(moi);
			selectionChanged = true;
		}
	}
	SelectedObjects = SelectedObjects.Difference(pendingDeselectObjects);

	auto *nextViewGroup = ViewGroupObject;
	while (nextViewGroup && nextViewGroup->IsDestroyed())
	{
		nextViewGroup = nextViewGroup->GetParentObject();
		viewChanged = true;
	}

	TArray<int32> invalidErrorIDs;
	for (auto &kvp : ObjectErrorMap)
	{
		int32 objID = kvp.Key;
		AModumateObjectInstance *obj = doc->GetObjectById(objID);
		if ((obj == nullptr) || obj->IsDestroyed())
		{
			invalidErrorIDs.Add(objID);
			viewChanged = true;
		}
	}
	for (int32 invalidObjID : invalidErrorIDs)
	{
		ObjectErrorMap.Remove(invalidObjID);
	}

	if (HoveredObject && HoveredObject->IsDestroyed())
	{
		HoveredObject = nullptr;
		viewChanged = true;
	}

	// Call PostSelectionChanged if either the set has been fixed, or we have selections
	// (just in case their selection state/handles change based on whatever just required re-validating selection state)
	if (selectionChanged || (SelectedObjects.Num() > 0))
	{
		PostSelectionChanged();
	}
	if (viewChanged)
	{
		PostViewChanged();
	}

	if (SelectedObjects.Num() > 0)
	{
		EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
	}

	return selectionChanged || viewChanged;
}

void AEditModelPlayerState::SelectAll()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::SelectAll"));

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;

	for (auto *selectedObj : SelectedObjects)
	{
		selectedObj->OnSelected(false);
	}
	SelectedObjects.Reset();

	for (auto *obj : LastReachableObjectSet)
	{
		if (obj)
		{
			SelectedObjects.Add(obj);
			obj->OnSelected(true);
		}
	}

	PostSelectionChanged();
	EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
}

void AEditModelPlayerState::SelectInverse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::SelectInverse"));

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;

	TSet<AModumateObjectInstance *> previousSelectedObjs(SelectedObjects);

	for (auto *selectedObj : SelectedObjects)
	{
		selectedObj->OnSelected(false);
	}
	SelectedObjects.Reset();

	for (auto *obj : doc->GetObjectInstances())
	{
		if ((obj->GetParentObject() == nullptr) &&
			!previousSelectedObjs.Contains(obj))
		{
			SelectedObjects.Add(obj);
			obj->OnSelected(true);
		}
	}

	PostSelectionChanged();
	EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
}

void AEditModelPlayerState::DeselectAll(bool bNotifyWidget)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::DeselectAll"));

	if (EMPlayerController->InteractionHandle)
	{
		EMPlayerController->InteractionHandle->AbortUse();
		EMPlayerController->InteractionHandle = nullptr;
	}

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;
	auto obs = doc->GetObjectInstances();

	for (auto &ob : obs)
	{
		ob->OnSelected(false);
	}

	SelectedObjects.Empty();
	SelectedGroupObjects.Empty();

	PostSelectionChanged();
	
	// Don't notify the widget if we're just deselecting in anticipation of selecting
	if (bNotifyWidget)
	{
		EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
	}
}

void AEditModelPlayerState::SetActorRenderValues(AActor* actor, int32 stencilValue, bool bNeverCull)
{
	USceneComponent *rootComp = actor ? actor->GetRootComponent() : nullptr;
	if (rootComp)
	{
		TArray<USceneComponent*> allComps;
		rootComp->GetChildrenComponents(true, allComps);
		allComps.Add(rootComp);

		for (USceneComponent* childComp : allComps)
		{
			if (UPrimitiveComponent *primitiveComp = Cast<UPrimitiveComponent>(childComp))
			{
				primitiveComp->SetRenderCustomDepth(stencilValue != 0);
				primitiveComp->SetCustomDepthStencilValue(stencilValue);

				// TODO: modify engine rendering to allow disabling culling without modifying bounds scale
				//primitiveComp->SetBoundsScale(boundsScale);
			}
		}
	}
}

void AEditModelPlayerState::PostSelectionChanged()
{
	if (!EMPlayerController || !EMPlayerController->CurrentTool)
	{
		return;
	}

	auto gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	bool bSelectTool = EMPlayerController->CurrentTool->GetToolMode() == EToolMode::VE_SELECT;
	if (SelectedObjects.Num() == 1 && bSelectTool)
	{
		AModumateObjectInstance* moi = *SelectedObjects.CreateConstIterator();
		if (!gameInstance->DimensionManager->CheckCanUseSameDimensionStrings(moi->ID))
		{
			gameInstance->DimensionManager->UpdateGraphDimensionStrings(moi->ID);
			moi->ShowAdjustmentHandles(EMPlayerController, true);
		}
	}
	else
	{
		gameInstance->DimensionManager->ClearGraphDimensionStrings();
		for (auto obj : LastSelectedObjectSet)
		{
			obj->ShowAdjustmentHandles(EMPlayerController, false);
		}
	}

	TSet<AModumateObjectInstance *> allChangedObjects;
	allChangedObjects.Append(SelectedObjects);
	allChangedObjects.Append(LastSelectedObjectSet);

	UpdateRenderFlags(allChangedObjects);

	LastSelectedObjectSet.Reset();
	LastSelectedObjectSet.Append(SelectedObjects);
}

void AEditModelPlayerState::PostViewChanged()
{
	// Gather a set of objects whose render mode may have changed due to either
	// selection, hover, or view group hierarchy
	TSet<AModumateObjectInstance *> allChangedObjects;

	// Gather all objects previously and currently hovered objects
	HoveredObjectDescendents.Reset();
	if (HoveredObject)
	{
		HoveredObjectDescendents.Add(HoveredObject);
		HoveredObjectDescendents.Append(HoveredObject->GetAllDescendents());
	}
	allChangedObjects.Append(HoveredObjectDescendents);
	allChangedObjects.Append(LastHoveredObjectSet);

	// Gather all objects previously and currently having errors
	auto* doc = GetWorld()->GetGameState<AEditModelGameState>()->Document;
	for (auto &kvp : ObjectErrorMap)
	{
		if (kvp.Value.Num() > 0)
		{
			if (auto *moi = doc->GetObjectById(kvp.Key))
			{
				ErrorObjects.Add(moi);
			}
		}
	}
	allChangedObjects.Append(ErrorObjects);
	allChangedObjects.Append(LastErrorObjectSet);

	// Iterate through all objects whose render mode may have changed due to either selection or view group hierarchy
	TArray<TWeakObjectPtr<AAdjustmentHandleActor>> adjustHandleActors;
	UpdateRenderFlags(allChangedObjects);

	LastHoveredObjectSet.Reset();
	LastHoveredObjectSet.Append(HoveredObjectDescendents);

	LastErrorObjectSet.Reset();
	LastErrorObjectSet.Append(ErrorObjects);

	// Also, use this opportunity to iterate through the scene graph and cache the set of objects
	// underneath the current view group (if it's set), and are not underneath a group.
	// TODO: only call this when necessary (when ViewGroupObject or the object list/hierarchy changes).
	FindReachableObjects(LastReachableObjectSet);
}

void AEditModelPlayerState::PostGroupChanged(const TArray<int32> ChangedGroups)
{
	if (!EMPlayerController || !EMPlayerController->CurrentTool)
	{
		return;
	}

	auto* gameState = GetWorld()->GetGameState<AEditModelGameState>();
	auto* doc = gameState->Document;

	TSet<AModumateObjectInstance*> changedObjects;
	UModumateObjectStatics::GetObjectsInGroups(doc, ChangedGroups, changedObjects);
	UpdateRenderFlags(changedObjects);
}

void AEditModelPlayerState::OnNewModel()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::OnNewModel"));
	if (EMPlayerController->ToolIsInUse())
	{
		EMPlayerController->AbortUseTool();
	}
	EMPlayerController->SetToolMode(EToolMode::VE_SELECT);
	SetViewMode(EEditViewModes::AllObjects);

	SnappedCursor.ClearAffordanceFrame();

	// Reset fields that have pointers/references to MOIs
	HoveredObject = nullptr;
	ViewGroupObject = nullptr;
	SelectedObjects.Reset();
	LastSelectedObjectSet.Reset();
	LastHoveredObjectSet.Reset();
	LastErrorObjectSet.Reset();
	LastReachableObjectSet.Reset();
	ObjectErrorMap.Reset();
	LastFilePath.Empty();

	PostViewChanged();

	auto gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	gameInstance->DimensionManager->Reset();
	gameInstance->ThumbnailCacheManager->ClearCachedThumbnails();

	PostOnNewModel.Broadcast();

	if (EMPlayerController && EMPlayerController->EditModelUserWidget)
	{
		EMPlayerController->EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::None);
		EMPlayerController->EditModelUserWidget->ToggleHelpMenu(false);
	}
}

void AEditModelPlayerState::SetObjectIDSelected(int32 ObjID, bool bSelected)
{
	auto* world = GetWorld();
	auto* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	AModumateObjectInstance* obj = (gameState && gameState->Document) ? gameState->Document->GetObjectById(ObjID) : nullptr;
	if (obj)
	{
		SetObjectSelected(obj, bSelected, bSelected);
	}
}

void AEditModelPlayerState::SetShowHoverEffects(bool showHoverEffects)
{
	ShowHoverEffects = showHoverEffects;
	PostViewChanged();
}

AModumateObjectInstance *AEditModelPlayerState::GetValidHoveredObjectInView(AModumateObjectInstance *hoverTarget) const
{
	AModumateObjectInstance *highestGroupUnderViewGroup = nullptr;
	AModumateObjectInstance *nextTarget = hoverTarget;
	bool newTargetInViewGroup = false;

	while (nextTarget && !newTargetInViewGroup)
	{
		if (nextTarget->GetObjectType() == EObjectType::OTGroup)
		{
			highestGroupUnderViewGroup = nextTarget;
		}
		nextTarget = nextTarget->GetParentObject();

		if (ViewGroupObject && (ViewGroupObject == nextTarget))
		{
			newTargetInViewGroup = true;
		}
	}

	bool validWithViewGroup = newTargetInViewGroup || !ViewGroupObject;

	if (validWithViewGroup)
	{
		if (highestGroupUnderViewGroup)
		{
			return highestGroupUnderViewGroup;
		}
		else
		{
			return hoverTarget;
		}
	}

	return nullptr;
}

void AEditModelPlayerState::SetHoveredObject(AModumateObjectInstance *ob)
{
//	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::SetHoveredObject"));
	if (HoveredObject != ob)
	{
		if (HoveredObject != nullptr)
		{
			HoveredObject->OnHovered(EMPlayerController, false);
		}
		HoveredObject = ob;
		if (ob != nullptr)
		{
			ob->OnHovered(EMPlayerController, true);
		}
		PostViewChanged();
	}
}

void AEditModelPlayerState::SetObjectsSelected(TSet<AModumateObjectInstance*>& Obs, bool bSelected, bool bDeselectOthers)
{
	if (bDeselectOthers)
	{
		DeselectAll(false);
	}

	for (auto& ob : Obs)
	{
		if (bSelected)
		{
			SelectedObjects.Add(ob);
		}
		else
		{
			SelectedObjects.Remove(ob);
		}
		ob->OnSelected(bSelected);
	}
	PostSelectionChanged();
	EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
}

void AEditModelPlayerState::SetGroupObjectSelected(AModumateObjectInstance* GroupObject, bool bSelected, bool bDeselectOthers)
{
	if (bDeselectOthers)
	{
		SelectedGroupObjects.Empty();
	}

	if (GroupObject)
	{
		if (bSelected)
		{
			SelectedGroupObjects.Add(GroupObject);
		}
		else
		{
			SelectedGroupObjects.Remove(GroupObject);
		}
	}
}

void AEditModelPlayerState::SetObjectSelected(AModumateObjectInstance *ob, bool bSelected, bool bDeselectOthers)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState::SetObjectSelected"));

	if (bSelected)
	{
		SelectedObjects.Add(ob);
	}
	else
	{
		SelectedObjects.Remove(ob);
	}

	ob->OnSelected(bSelected);

	PostSelectionChanged();
	EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
}

void AEditModelPlayerState::SetViewGroupObject(AModumateObjectInstance *ob)
{
	ViewGroupObject = ob;
	PostViewChanged();
}

void AEditModelPlayerState::FindReachableObjects(TSet<AModumateObjectInstance*> &reachableObjs) const
{
	reachableObjs.Reset();

	auto *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	auto* doc = gameState->Document;
	const auto &allObjects = doc->GetObjectInstances();
	int32 viewGroupObjID = ViewGroupObject ? ViewGroupObject->ID : 0;

	TQueue<AModumateObjectInstance *> objQueue;

	for (auto *obj : allObjects)
	{
		if ((obj->GetParentID() == viewGroupObjID) && obj->IsSelectableByUser())
		{
			objQueue.Enqueue(obj);
		}
	}

	TSet<AModumateObjectInstance *> visited;
	AModumateObjectInstance *iter = nullptr;

	while (objQueue.Dequeue(iter))
	{
		visited.Add(iter);
		reachableObjs.Add(iter);

		if (iter->GetObjectType() != EObjectType::OTGroup)
		{
			auto iterChildren = iter->GetChildObjects();
			for (auto *iterChild : iterChildren)
			{
				if (iterChild && !visited.Contains(iterChild) && iterChild->IsSelectableByUser())
				{
					objQueue.Enqueue(iterChild);
				}
			}
		}
	}
}

bool AEditModelPlayerState::IsObjectReachableInView(AModumateObjectInstance* obj) const
{
	return LastReachableObjectSet.Contains(obj);
}

AActor* AEditModelPlayerState::GetViewGroupObject() const
{
	return ViewGroupObject ? ViewGroupObject->GetActor() : nullptr;
}

AActor* AEditModelPlayerState::GetHoveredObject() const
{
	return HoveredObject ? HoveredObject->GetActor() : nullptr;
}

bool AEditModelPlayerState::SetErrorForObject(int32 ObjectID, FName ErrorTag, bool bIsError)
{
	TSet<FName> &objectErrors = ObjectErrorMap.FindOrAdd(ObjectID);

	bool bChanged = false;

	if (bIsError)
	{
		objectErrors.Add(ErrorTag, &bChanged);
	}
	else
	{
		bChanged = (objectErrors.Remove(ErrorTag) > 0);
	}

	if (bChanged)
	{
		PostViewChanged();
	}

	return bChanged;
}

bool AEditModelPlayerState::ClearErrorsForObject(int32 ObjectID)
{
	TSet<FName> *objectErrors = ObjectErrorMap.Find(ObjectID);

	if (objectErrors && (objectErrors->Num() > 0))
	{
		objectErrors->Reset();
		PostViewChanged();
		return true;
	}

	return false;
}

bool AEditModelPlayerState::DoesObjectHaveError(int32 ObjectID, FName ErrorTag) const
{
	const TSet<FName> *objectErrors = ObjectErrorMap.Find(ObjectID);
	return (objectErrors && objectErrors->Contains(ErrorTag));
}

bool AEditModelPlayerState::DoesObjectHaveAnyError(int32 ObjectID) const
{
	const TSet<FName> *objectErrors = ObjectErrorMap.Find(ObjectID);
	return (objectErrors && (objectErrors->Num() > 0));
}

void AEditModelPlayerState::CopySelectedToClipboard(const UModumateDocument &document)
{
	// TODO: re-implement
}

void AEditModelPlayerState::Paste(UModumateDocument &document) const
{
	// TODO: re-implement
}

FGuid AEditModelPlayerState::GetAssemblyForToolMode(EToolMode mode)
{
	TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(mode);
	if (ensureAlways(tool))
	{
		return tool->GetAssemblyGUID();
	}
	return FGuid();
}

void AEditModelPlayerState::SetAssemblyForToolMode(EToolMode Mode, const FGuid& AssemblyGUID)
{
	TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(Mode);
	if (ensureAlways(tool))
	{
		tool->SetAssemblyGUID(AssemblyGUID);
	}
}

bool AEditModelPlayerState::SetViewMode(EEditViewModes NewEditViewMode, bool bForceUpdate)
{
	if (((SelectedViewMode != NewEditViewMode) || bForceUpdate) && EMPlayerController && EMPlayerController->ValidViewModes.Contains(NewEditViewMode))
	{
		SelectedViewMode = NewEditViewMode;
		EMPlayerController->UpdateMouseTraceParams();
		if (EMPlayerController->EditModelUserWidget != nullptr)
		{
			EMPlayerController->EditModelUserWidget->UpdateViewModeIndicator(NewEditViewMode);
		}

		// Update global meta plane material params
		if (MetaPlaneMatParamCollection)
		{
			float metaPlaneAlphaValue = (SelectedViewMode == EEditViewModes::MetaGraph) ? 1.0f : 0.75f;
			static const FName metaPlaneAlphaParamName(TEXT("MetaPlaneAlpha"));
			UMaterialParameterCollectionInstance* metaPlaneMatParamInst = GetWorld()->GetParameterCollectionInstance(MetaPlaneMatParamCollection);
			metaPlaneMatParamInst->SetScalarParameterValue(metaPlaneAlphaParamName, metaPlaneAlphaValue);
		}

		UpdateObjectVisibilityAndCollision();
		return true;
	}

	return false;
}

bool AEditModelPlayerState::ChangeViewMode(int32 IndexDelta)
{
	IndexDelta = FMath::Sign(IndexDelta);
	if (IndexDelta == 0)
	{
		return false;
	}

	const auto& validViewModes = EMPlayerController->ValidViewModes;
	int32 numViewModes = validViewModes.Num();
	if (numViewModes == 1)
	{
		return true;
	}

	int32 curViewModeIndex = validViewModes.Find(SelectedViewMode);
	if (ensure(curViewModeIndex != INDEX_NONE))
	{
		// Cycle forward or backwards through the valid edit modes,
		// but skip Physical edit mode because it doesn't follow the hierarchy pattern that the rest of them do.
		EEditViewModes newViewMode = SelectedViewMode;
		do
		{
			curViewModeIndex = (curViewModeIndex + IndexDelta + numViewModes) % numViewModes;
			newViewMode = validViewModes[curViewModeIndex];
		} while (newViewMode == EEditViewModes::Physical);

		return SetViewMode(newViewMode);
	}
	return false;
}

void AEditModelPlayerState::UpdateObjectVisibilityAndCollision()
{
	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;
	for (AModumateObjectInstance *moi : doc->GetObjectInstances())
	{
		if (moi)
		{
			moi->MarkDirty(EObjectDirtyFlags::Visuals);
		}
	}
}

bool AEditModelPlayerState::IsObjectTypeEnabledByViewMode(EObjectType ObjectType) const
{
	if (SelectedViewMode == EEditViewModes::AllObjects)
	{
		return true;
	}

	EToolCategories objectCategory = UModumateTypeStatics::GetObjectCategory(ObjectType);
	switch (objectCategory)
	{
	case EToolCategories::MetaGraph:
		return (SelectedViewMode != EEditViewModes::Physical);
	case EToolCategories::Separators:
		return (SelectedViewMode != EEditViewModes::MetaGraph);
	case EToolCategories::SurfaceGraphs:
		return (SelectedViewMode == EEditViewModes::SurfaceGraphs) || (SelectedViewMode == EEditViewModes::AllObjects);
	case EToolCategories::Attachments:
		return (SelectedViewMode == EEditViewModes::AllObjects) || (SelectedViewMode == EEditViewModes::Physical);
	case EToolCategories::SiteTools:
		return true;
	case EToolCategories::Unknown:
		switch (ObjectType)
		{
		case EObjectType::OTRoom:
			return bRoomsVisible;
		case EObjectType::OTCutPlane:
		case EObjectType::OTScopeBox:
		case EObjectType::OTBackgroundImage:
		case EObjectType::OTTerrain:
		case EObjectType::OTTerrainVertex:
		case EObjectType::OTTerrainEdge:
		case EObjectType::OTTerrainPolygon:
			return true;
		case EObjectType::OTEdgeDetail:
		case EObjectType::OTMetaGraph:
			return false;
		default:
			ensureMsgf(false, TEXT("Unhandled uncategorized object type: %s!"), *GetEnumValueString(ObjectType));
			return false;
		}
	default:
		ensureMsgf(false, TEXT("Unknown object category: %d!"), (int8)objectCategory);
		return false;
	}
}

void AEditModelPlayerState::ClearConflictingRedoBuffer(const FDeltasRecord& Deltas)
{
	if (UndoneDeltasRecords.Num() == 0)
	{
		return;
	}

	// Gather the set of all objects, presets, and locations affected by the deltas in this player's redo stack.
	TSet<int32> affectedObjects;
	TSet<FGuid> affectedPresets;
	FBox affectedBounds;
	for (const auto& undoneRecord : UndoneDeltasRecords)
	{
		undoneRecord.GatherResults(affectedObjects, affectedPresets, &affectedBounds);
	}

	// If the incoming delta had no effects that conflict with any of the undone deltas, then clear the redo buffer.
	if (Deltas.ConflictsWithResults(affectedObjects, affectedPresets, &affectedBounds))
	{
		UndoneDeltasRecords.Reset();
	}
}

FLinearColor AEditModelPlayerState::GetClientColor() const
{
	if (MultiplayerClientIdx == INDEX_NONE)
	{
		return DefaultPlayerColor;
	}

	// Golden Ratio % 1.0 happens to allow cycling through repeating intervals with optimal coverage,
	// to select an arbitrary number of hues that are initially spaced out and remain spaced out.
	// TODO: select from a pre-authored table of colors, and allow players to customize if similar colors are okay.
	FLinearColor assignedColorHSV = DefaultPlayerColor.LinearRGBToHSV();
	assignedColorHSV.R = 360.0f * FMath::Frac((assignedColorHSV.R / 360.0f) + (MultiplayerClientIdx * UE_GOLDEN_RATIO));
	return assignedColorHSV.HSVToLinearRGB();
}

void AEditModelPlayerState::UpdateAllUsersList()
{
	ULocalPlayer* localPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	auto* controller = localPlayer ? Cast<AEditModelPlayerController>(localPlayer->GetPlayerController(GetWorld())) : nullptr;
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->UpdateUsersList();
	}
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::SetUserInfo_Implementation(const FModumateUserInfo& UserInfo, int32 ClientIdx)
{
	ReplicatedUserInfo = UserInfo;
	MultiplayerClientIdx = ClientIdx;

	UWorld* world = GetWorld();
	AEditModelGameMode* gameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
	if (gameMode && EMPlayerController)
	{
		gameMode->ChangeName(EMPlayerController, ReplicatedUserInfo.Firstname, true);
	}
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::SendClientDeltas_Implementation(FDeltasRecord Deltas)
{
	if (!ReplicatedProjectPermissions.CanEdit)
	{
		UE_LOG(LogTemp, Warning, TEXT("User without project edit permissions sent delta, userId=%s"), *ReplicatedUserInfo.ID);
		return;
	}

	FString userID;
	UWorld* world = GetWorld();
	AEditModelGameState* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	AEditModelGameMode* gameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
	if (gameState && gameState->Document && gameMode && !Deltas.IsEmpty() && gameMode->GetUserByPlayerID(GetPlayerId(), userID))
	{
		// If the client sent deltas before the server finished downloading the cloud document, or while another user is logging in, then reject them immediately.
		if (gameState->bDownloadingDocument)
		{
			UE_LOG(LogTemp, Warning, TEXT("User %s sent DeltasRecord %08x before the server was ready - rolling back to 0."),
				*Deltas.OriginUserID, Deltas.TotalHash);
			RollBackUnverifiedDeltas(0);
		}
		else if (gameMode->IsAnyUserPendingLogin())
		{
			uint32 verifiedDocHash = gameState->Document->GetLatestVerifiedDocHash();
			UE_LOG(LogTemp, Log, TEXT("User %s sent DeltasRecord %08x while someone was logging in - rolling back to %08x."),
				*Deltas.OriginUserID, Deltas.TotalHash, verifiedDocHash);
			RollBackUnverifiedDeltas(verifiedDocHash);
		}
		else
		{
			// Clear the user's redo buffer if they're sending a new non-redo record
			UndoneDeltasRecords.Reset();

			// Expand the delta's bounds based on the current bounds of objects that the deltas are about to affect,
			// rather than what's serialized, which is only the bounds of objects that the deltas have already affected.
			Deltas.AffectedObjBounds += gameState->Document->GetAffectedBounds(Deltas.AffectedObjectsMap, Deltas.DirtiedObjectsSet);

			// If the server verifies that these deltas have been made after the latest one,
			// then broadcast them to every client.
			FDeltasRecord reconciledRecord;
			uint32 verifiedDocHash;
			if (gameState->Document->ReconcileRemoteDeltas(Deltas, world, reconciledRecord, verifiedDocHash))
			{
				UE_LOG(LogTemp, Log, TEXT("Broadcasting verified DeltasRecord %08x from user %s"), Deltas.TotalHash, *Deltas.OriginUserID);
				gameState->BroadcastServerDeltas(userID, Deltas);
			}
			// Otherwise, tell the client that it needs to roll back all unverified deltas,
			// and potentially broadcast reconciled deltas that include changes from the out-of-date client's deltas.
			else
			{
				// TODO: if there *is* a reconciled record with identical deltas, we could send a new specialized message to the client
				// not to roll *back* the unverified delta, but just to *reorder* it with a new previous document hash.
				// This would significantly save performance on clients that are rolling back and then redoing large deltas that are prone
				// to being reconciled in the first place, like big graph cuts or large moves.
				UE_LOG(LogTemp, Warning, TEXT("User %s sent out-of-date DeltasRecord hash %08x - rolling back"), *Deltas.OriginUserID, Deltas.TotalHash);
				RollBackUnverifiedDeltas(verifiedDocHash);

				if (!reconciledRecord.IsEmpty())
				{
					UE_LOG(LogTemp, Log, TEXT("Broadcasting reconciled DeltasRecord DeltasRecord %08x from user %s"), Deltas.TotalHash, *Deltas.OriginUserID);
					gameState->BroadcastServerDeltas(userID, reconciledRecord);
				}
			}
		}
	}
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::TryUndo_Implementation()
{
	FString userID;
	TArray<uint32> undoRecordHashes;

	UWorld* world = GetWorld();
	AEditModelGameState* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	AEditModelGameMode* gameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
	if (gameState && gameState->Document && gameMode && gameMode->GetUserByPlayerID(GetPlayerId(), userID))
	{
		if (gameState->bDownloadingDocument)
		{
			UE_LOG(LogTemp, Warning, TEXT("User %s tried to undo while the server was still downloading the project - rejecting"), *userID);
		}
		else if (gameMode->IsAnyUserPendingLogin())
		{
			UE_LOG(LogTemp, Log, TEXT("User %s tried to undo while someone is logging in to the server - rejecting"), *userID);
		}
		else if (gameState->Document->GetUndoRecordsFromClient(world, userID, undoRecordHashes))
		{
			// Add the undone records to our redo stack
			auto& verifiedDeltas = gameState->Document->GetVerifiedDeltasRecords();
			int32 numUndoRecords = undoRecordHashes.Num();
			for (int32 recordIDIdx = numUndoRecords - 1; recordIDIdx >= 0; --recordIDIdx)
			{
				uint32 undoneRecordHash = undoRecordHashes[recordIDIdx];
				int32 recordIdx = gameState->Document->FindDeltasRecordIdxByHash(undoneRecordHash);
				if (!ensure(recordIdx != INDEX_NONE))
				{
					break;
				}

				auto& undoneRecord = verifiedDeltas[recordIdx];
				UndoneDeltasRecords.Add(undoneRecord);
			}

			gameState->BroadcastUndo(userID, undoRecordHashes);
		}
	}
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::TryRedo_Implementation()
{
	FString userID;
	TArray<uint32> undoRecordHashes;

	UWorld* world = GetWorld();
	AEditModelGameState* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	AEditModelGameMode* gameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
	if (gameState && gameState->Document && gameMode && gameMode->GetUserByPlayerID(GetPlayerId(), userID))
	{
		if (gameState->bDownloadingDocument)
		{
			UE_LOG(LogTemp, Warning, TEXT("User %s tried to redo while the server was still downloading the project - rejecting"), *userID);
		}
		else if (gameMode->IsAnyUserPendingLogin())
		{
			UE_LOG(LogTemp, Log, TEXT("User %s tried to redo while someone is logging in to the server - rejecting"), *userID);
		}
		else if (UndoneDeltasRecords.Num() > 0)
		{
			FDeltasRecord redoRecord = UndoneDeltasRecords.Pop();
			redoRecord.PrevDocHash = gameState->Document->GetLatestVerifiedDocHash();
			redoRecord.ComputeHash();

			UE_LOG(LogTemp, Log, TEXT("Broadcasting redone DeltasRecord %08x from user %s"), redoRecord.TotalHash, *userID);
			gameState->BroadcastServerDeltas(userID, redoRecord, true);
		}
	}
}

// RPC from the server to the client's copy of its PlayerState
void AEditModelPlayerState::SendInitialState_Implementation(const FString& ProjectID, int32 UserIdx, uint32 CurDocHash)
{
	UE_LOG(LogTemp, Log, TEXT("Received initial state for Project ID %s; User #%d, expected doc hash: %08x."), *ProjectID, UserIdx, CurDocHash);

	CurProjectID = ProjectID;
	MultiplayerClientIdx = UserIdx;
	bPendingClientDownload = true;
	ExpectedDownloadDocHash = CurDocHash;

	// Now that we have a client index and project ID, we should be able to initialize the controller, player state, and document;
	// we shouldn't have been able to before now, but if we still can't at this frame, the controller will do so on its BeginPlay.
	TryInitController();
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::OnDownloadedDocument_Implementation(uint32 DownloadedDocHash)
{
	FString userID;
	UWorld* world = GetWorld();
	AEditModelGameMode* gameMode = world ? world->GetAuthGameMode<AEditModelGameMode>() : nullptr;
	if (gameMode && gameMode->GetUserByPlayerID(GetPlayerId(), userID))
	{
		gameMode->OnUserDownloadedDocument(userID, DownloadedDocHash);
	}
}

// RPC from the server to the client's copy of its PlayerState
void AEditModelPlayerState::RollBackUnverifiedDeltas_Implementation(uint32 VerifiedDocHash)
{
	UWorld* world = GetWorld();
	AEditModelGameState* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	if (gameState && gameState->Document)
	{
		gameState->Document->RollBackUnverifiedDeltas(VerifiedDocHash, world);
	}
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::UpdateCameraUnreliable_Implementation(const FTransform& NewTransform)
{
	ReplicatedCamTransform = NewTransform;
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::UpdateCameraReliable_Implementation(const FTransform& NewTransform)
{
	ReplicatedCamTransform = NewTransform;
}

void AEditModelPlayerState::UpdateCursorLocationUnreliable_Implementation(const FVector& NewLocation)
{
	ReplicatedCursorLocation = NewLocation;
}

// RPC from the client to the server's copy of that client's PlayerState
void AEditModelPlayerState::RequestUpload_Implementation()
{
	UWorld* world = GetWorld();
	AEditModelGameState* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	if (gameState)
	{
		gameState->UploadDocument();
	}
}

void AEditModelPlayerState::UploadProjectThumbnail_Implementation(const FString& Base64String)
{
	UWorld* world = GetWorld();
	AEditModelGameState* gameState = world ? world->GetGameState<AEditModelGameState>() : nullptr;
	if (gameState && Base64String.Len() > 0)
	{
		TArray<uint8> thumbImage;
		if (ensure(FBase64::Decode(Base64String, thumbImage)))
		{
			gameState->UploadThumbnail(thumbImage);
		}
	}
}

void AEditModelPlayerState::OnRep_CamTransform()
{
	UWorld* world = GetWorld();
	AEditModelPlayerPawn* playerPawn = GetPawn<AEditModelPlayerPawn>();
	ULocalPlayer* localPlayer = EMPlayerController ? EMPlayerController->GetLocalPlayer() : nullptr;

	if (world && playerPawn && (localPlayer == nullptr) && IsNetMode(NM_Client))
	{
		CamReplicationTransformBuffer.AddToTransformBuffer(world->GetTimeSeconds(), ReplicatedCamTransform);
	}
}

void AEditModelPlayerState::OnRep_CursorLocation()
{
	UWorld* world = GetWorld();
	AEditModelPlayerPawn* playerPawn = GetPawn<AEditModelPlayerPawn>();
	ULocalPlayer* localPlayer = EMPlayerController ? EMPlayerController->GetLocalPlayer() : nullptr;

	if (world && playerPawn && (localPlayer == nullptr) && IsNetMode(NM_Client))
	{
		CursorReplicationTransformBuffer.AddToTransformBuffer(world->GetTimeSeconds(), FTransform(ReplicatedCursorLocation));
	}
}

void AEditModelPlayerState::OnRep_UserInfo()
{
	// TODO: download and update other player's profile photos
	UE_LOG(LogTemp, Log, TEXT("Replicated %s's user info, id: %s"), *ReplicatedUserInfo.Firstname, *ReplicatedUserInfo.ID);

	UpdateAllUsersList();
}

void AEditModelPlayerState::OnRep_UserPermissions()
{
	UE_LOG(LogTemp, Log, TEXT("Permissions changed: view=%d, edit=%d, chat=%d, export=%d"), 
		ReplicatedProjectPermissions.CanView, 
		ReplicatedProjectPermissions.CanEdit,
		ReplicatedProjectPermissions.CanChat,
		ReplicatedProjectPermissions.CanExport);

	ProjectPermChangedEvent.Broadcast();
	
}

void AEditModelPlayerState::UpdateOtherClientCameraAndCursor()
{
	UWorld* world = GetWorld();
	AEditModelPlayerPawn* playerPawn = GetPawn<AEditModelPlayerPawn>();
	ULocalPlayer* localPlayer = EMPlayerController ? EMPlayerController->GetLocalPlayer() : nullptr;

	// Check if this player state belongs to another client
	if (!(world && playerPawn && (localPlayer == nullptr) && IsNetMode(NM_Client)))
	{
		return;
	}

	// Buffer the camera transform so that a smooth value can be calculated upon update from tick
	FTransform blendedCamTransform;
	if (CamReplicationTransformBuffer.GetBlendedTransform(world->GetTimeSeconds(), blendedCamTransform))
	{
		playerPawn->SetActorTransform(blendedCamTransform);
	}
	else 
	{
		// If GetBlendedTransform failed (ex: buffer is <= 1), then immediately update the other client's player pawn camera mesh;	
		// this will appear jittery and low-fps without having much higher replication speed.
		playerPawn->SetActorTransform(ReplicatedCamTransform);
	}

	// Cursor location blend. Same logic as blending camera transform
	FTransform blendedCursorTransform;
	if (CursorReplicationTransformBuffer.GetBlendedTransform(world->GetTimeSeconds(), blendedCursorTransform))
	{
		InterpReplicatedCursorLocation = blendedCursorTransform.GetLocation();
	}
	else
	{
		InterpReplicatedCursorLocation = ReplicatedCursorLocation;
	}
}

FProjectPermissions AEditModelPlayerState::ParseProjectPermissions(const TArray<FString>& Permissions)
{
	FProjectPermissions userPerms;
	userPerms.CanView = false;
	userPerms.CanEdit = false;
	userPerms.CanChat = false;
	userPerms.CanExport = false;

	for (auto& permission : Permissions)
	{
		if (permission.Contains(TEXT("project.*")))
		{
			UE_LOG(LogTemp, Log, TEXT("Permission: project.*"));
			userPerms.CanView = true;
			userPerms.CanEdit = true;
			userPerms.CanChat = true;
			userPerms.CanExport = true;
		}
		else if (permission.Contains(TEXT("project.view")))
		{
			UE_LOG(LogTemp, Log, TEXT("Permission: project.view"));
			userPerms.CanView = true;
		}
		else if (permission.Contains(TEXT("project.edit")))
		{
			UE_LOG(LogTemp, Log, TEXT("Permission: project.edit"));
			userPerms.CanEdit = true;
		}
		else if (permission.Contains(TEXT("project.chat")))
		{
			UE_LOG(LogTemp, Log, TEXT("Permission: project.chat"));
			userPerms.CanChat = true;
		}
		else if (permission.Contains(TEXT("project.export")))
		{
			UE_LOG(LogTemp, Log, TEXT("Permission: project.export"));
			userPerms.CanExport = true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Unknown permissions detected: %s"), *permission);
		}
	}

	return userPerms;
}

void AEditModelPlayerState::SetupPermissions_Implementation()
{
	FProjectPermissions defaultPermissions;

	defaultPermissions.CanView = true;
	defaultPermissions.CanChat = false;
	defaultPermissions.CanEdit = false;
	defaultPermissions.CanExport = false;

	ReplicatedProjectPermissions = defaultPermissions;

	AEditModelGameMode* gameMode = Cast<AEditModelGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	
	if (gameMode)
	{
		RequestPermissions(ReplicatedUserInfo.ID, gameMode->GetCurProjectID(),
			[&](const FString& user, const FString& project, const FProjectPermissions& perms)
			{
				this->ReplicatedProjectPermissions = perms;
				UE_LOG(LogTemp, Log, TEXT("Setting Permissions, canEdit=%d"), this->ReplicatedProjectPermissions.CanEdit);
			}
		);
	}
}

bool AEditModelPlayerState::RequestPermissions(const FString& UserID, const FString& ProjectID,
	const TFunction<void(const FString& user, const FString& project, const FProjectPermissions& perms)>& Callback)
{
	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	auto cloudConnection = gameInstance ? gameInstance->GetCloudConnection() : nullptr;


	FString getConnectionEndpoint = FProjectConnectionHelpers::MakeProjectConnectionEndpoint(ProjectID) / UserID;
	UE_LOG(LogTemp, Log, TEXT("Checking endpoint, %s"), *getConnectionEndpoint);

	bool bQuerySuccess = cloudConnection->RequestEndpoint(getConnectionEndpoint, FModumateCloudConnection::ERequestType::Get, nullptr,
		[=](bool bSuccess, const TSharedPtr<FJsonObject>& Payload)
		{
			UE_LOG(LogTemp, Log, TEXT("Setting user permissions, user ID=%s"), *UserID);
			FProjectConnectionResponse getConnectionResponse;
			if (!ensure(bSuccess && FJsonObjectConverter::JsonObjectToUStruct(Payload.ToSharedRef(), &getConnectionResponse)))
			{
				return;
			}

			FProjectPermissions userPerms = AEditModelPlayerState::ParseProjectPermissions(getConnectionResponse.Permissions);
			if (Callback)
			{
				Callback(UserID, ProjectID, userPerms);
			}

		},
		[=](int32 ErrorCode, const FString& ErrorString)
		{
			FProjectPermissions userPerms;

#if UE_BUILD_SHIPPING
			//Revoke non-view permissions on failure in shipping.
			UE_LOG(LogTemp, Error, TEXT("Error code %d querying permissions for User ID %s: %s"),
				ErrorCode, *UserID, *ErrorString);

			userPerms.CanView = true;
			userPerms.CanEdit = false;
			userPerms.CanChat = false;
			userPerms.CanExport = false;
#else
			bool viewOnly = FParse::Param(FCommandLine::Get(), *AEditModelPlayerState::ViewOnlyArg);

			UE_LOG(LogTemp, Warning, TEXT("Unable to query permissions -- standalone windows server detected. Setting viewOnly=%d"), viewOnly);
			userPerms.CanView = true;
			userPerms.CanEdit = !viewOnly;
			userPerms.CanChat = !viewOnly;
			userPerms.CanExport = !viewOnly;
#endif
			if (Callback)
			{
				Callback(UserID, ProjectID, userPerms);
			}
		},
			false);

	return true;
}
