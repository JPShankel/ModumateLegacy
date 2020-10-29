// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelPlayerState_CPP.h"

#include "Database/ModumateObjectEnums.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/DimensionManager.h"
#include "UnrealClasses/LineActor.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Database/ModumateObjectDatabase.h"
#include "Algo/Transform.h"
#include "UI/EditModelUserWidget.h"


using namespace Modumate;

AEditModelPlayerState_CPP::AEditModelPlayerState_CPP()
	: ShowDebugSnaps(false)
	, ShowDocumentDebug(false)
	, bShowGraphDebug(false)
	, bShowVolumeDebug(false)
	, bShowSurfaceDebug(false)
	, bDevelopDDL2Data(false)
	, SelectedViewMode(EEditViewModes::ObjectEditing)
	, ShowingFileDialog(false)
	, HoveredObject(nullptr)
	, DebugMouseHits(false)
	, bShowSnappedCursor(true)
	, ShowHoverEffects(false)
{
	//DimensionString.Visible = false;
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::AEditModelPlayerState_CPP"));

	PrimaryActorTick.bCanEverTick = true;
}

void AEditModelPlayerState_CPP::BeginPlay()
{
	Super::BeginPlay();

	// This is supposed to be set by the EMPlayerController,
	// but it can be null if we spawn a new PlayerController that still uses this EMPlayerState.
	// (For example, the DebugCameraController, spawned by the CheatManager when pressing the semicolon key in non-shipping builds)
	if (EMPlayerController == nullptr)
	{
		EMPlayerController = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
		check(EMPlayerController);
	}

	PostSelectionChanged();
	PostViewChanged();
	SetEditMode(EEditViewModes::ObjectEditing, true);
}

void AEditModelPlayerState_CPP::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ensureMsgf(EMPlayerController != nullptr,
		TEXT("AEditModelPlayerController_CPP should have initialized AEditModelPlayerState_CPP!")))
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

void AEditModelPlayerState_CPP::BatchRenderLines()
{
	CurSelectionStructurePoints.Reset();
	CurSelectionStructureLines.Reset();
	CurViewGroupObjects.Reset();

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument &doc = gameState->Document;

	if (ViewGroupObject)
	{
		FModumateObjectInstance *curViewGroupObj = ViewGroupObject;
		while (curViewGroupObj)
		{
			CurViewGroupObjects.Add(curViewGroupObj);
			curViewGroupObj->GetStructuralPointsAndLines(TempObjectStructurePoints, TempObjectStructureLines);
			CurSelectionStructurePoints.Append(TempObjectStructurePoints);
			CurSelectionStructureLines.Append(TempObjectStructureLines);
			curViewGroupObj = curViewGroupObj->GetParentObject();
		}
	}

	if ((SelectedObjects.Num() > 0))
	{
		for (const auto *selectedObj : SelectedObjects)
		{
			if (selectedObj && selectedObj->ShowStructureOnSelection())
			{
				selectedObj->GetStructuralPointsAndLines(TempObjectStructurePoints, TempObjectStructureLines);
				CurSelectionStructurePoints.Append(TempObjectStructurePoints);
				CurSelectionStructureLines.Append(TempObjectStructureLines);
			}
		}
	}

	for (auto &structureLine : CurSelectionStructureLines)
	{
		FModumateObjectInstance *lineObj = doc.GetObjectById(structureLine.ObjID);
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
			objectSelectionLine.Color = FLinearColor(FColor(0x1C, 0x9F, 0xFF));
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
	for (FModumateObjectInstance *cutPlaneObj : doc.GetObjectsOfType(EObjectType::OTCutPlane))
	{
		if (!cutPlaneObj || !cutPlaneObj->IsVisible())
		{
			continue;
		}

		cutPlaneObj->AddDraftingLines(EMPlayerController->HUDDrawWidget);
	}

	for (FModumateObjectInstance *scopeBoxObj : doc.GetObjectsOfType(EObjectType::OTScopeBox))
	{
		if (!scopeBoxObj || !scopeBoxObj->IsVisible())
		{
			continue;
		}

		scopeBoxObj->AddDraftingLines(EMPlayerController->HUDDrawWidget);
	}
}

void AEditModelPlayerState_CPP::UpdateRenderFlags(const TSet<FModumateObjectInstance*>& ChangedObjects)
{
	if (ChangedObjects.Num() > 0)
	{
		OnSelectionOrViewChanged.Broadcast();
	}

	for (FModumateObjectInstance *moi : ChangedObjects)
	{
		if (moi && !moi->IsDestroyed())
		{
			auto type = moi->GetObjectType();
			bool bIsGraphType = (UModumateTypeStatics::Graph2DObjectTypeFromObjectType(type) != Modumate::EGraphObjectType::None) ||
				(UModumateTypeStatics::Graph3DObjectTypeFromObjectType(type) != Modumate::EGraph3DObjectType::None);
			bool bHovered = (ShowHoverEffects && !bIsGraphType && HoveredObjectDescendents.Contains(moi));

			int32 selectionValue = SelectedObjects.Contains(moi) ? 0x1 : 0x0;
			int32 viewGroupValue = ViewGroupDescendents.Contains(moi) ? 0x2 : 0x0;
			int32 hoverValue = bHovered ? 0x4 : 0x0;
			int32 errorValue = ErrorObjects.Contains(moi) ? 0x8 : 0x0;
			int32 stencilValue = selectionValue | viewGroupValue | hoverValue | errorValue;

			SetActorRenderValues(moi->GetActor(), stencilValue, bHovered);
		}
	}
}

void AEditModelPlayerState_CPP::SetShowGraphDebug(bool bShow)
{
	if (bShowGraphDebug != bShow)
	{
		bShowGraphDebug = bShow;

		// TODO: replace with non-debug visibility flag for rooms; for now, just piggypack off of the room graph visibility
		AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
		FModumateDocument &doc = gameState->Document;
		TArray<FModumateObjectInstance *> rooms = doc.GetObjectsOfType(EObjectType::OTRoom);
		for (FModumateObjectInstance *room : rooms)
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

AEditModelGameMode_CPP *AEditModelPlayerState_CPP::GetEditModelGameMode()
{
	return Cast<AEditModelGameMode_CPP>(GetWorld()->GetAuthGameMode());
}

void AEditModelPlayerState_CPP::ToggleRoomViewMode()
{
	if (SelectedViewMode == EEditViewModes::Rooms)
	{
		SetEditMode(PreviousModeFromToggleRoomView);
	}
	else
	{
		PreviousModeFromToggleRoomView = SelectedViewMode;
		SetEditMode(EEditViewModes::Rooms);
	}
}

bool AEditModelPlayerState_CPP::GetSnapCursorDeltaFromRay(const FVector& RayOrigin, const FVector& RayDir, FVector& OutPosition) const
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

void AEditModelPlayerState_CPP::DebugShowWallProfiles(const TArray<FModumateObjectInstance *> &walls)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::DebugShowWallProfiles"));
}

bool AEditModelPlayerState_CPP::ValidateSelectionsAndView()
{
	// TODO: revisit the necessity of this function, because it basically clears out any pointers to destroyed MOIs,
	// which should be more consistently + correctly handled by some different pointer scheme.
	// Most other parts of the codebase that point to FModumateObjectInstances assume that they clear out their pointers
	// as soon as they become invalid (i.e. deleting objects in a parent-child hierarchy).

	bool selectionChanged = false;
	bool viewChanged = false;

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;

	TSet<FModumateObjectInstance*> pendingDeselectObjects;
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
		FModumateObjectInstance *obj = doc->GetObjectById(objID);
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

	if (selectionChanged)
	{
		PostSelectionChanged();
	}
	if (viewChanged)
	{
		PostViewChanged();
	}

	return selectionChanged || viewChanged;
}

void AEditModelPlayerState_CPP::SelectAll()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SelectAll"));

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;

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

void AEditModelPlayerState_CPP::SelectInverse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SelectInverse"));

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;

	TSet<FModumateObjectInstance *> previousSelectedObjs(SelectedObjects);

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

void AEditModelPlayerState_CPP::DeselectAll()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::DeselectAll"));

	if (EMPlayerController->InteractionHandle)
	{
		EMPlayerController->InteractionHandle->AbortUse();
		EMPlayerController->InteractionHandle = nullptr;
	}

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;
	auto obs = doc->GetObjectInstances();

	for (auto &ob : obs)
	{
		ob->OnSelected(false);
	}

	SelectedObjects.Empty();

	PostSelectionChanged();
	EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
}

void AEditModelPlayerState_CPP::SetActorRenderValues(AActor* actor, int32 stencilValue, bool bNeverCull)
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

void AEditModelPlayerState_CPP::PostSelectionChanged()
{
	auto gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	if (SelectedObjects.Num() == 1)
	{
		FModumateObjectInstance* moi = *SelectedObjects.CreateConstIterator();

		gameInstance->DimensionManager->UpdateGraphDimensionStrings(moi->ID);
		moi->ShowAdjustmentHandles(EMPlayerController, true);
	}
	else
	{
		gameInstance->DimensionManager->ClearGraphDimensionStrings();
		for (auto obj : LastSelectedObjectSet)
		{
			obj->ShowAdjustmentHandles(EMPlayerController, false);
		}
	}

	TSet<FModumateObjectInstance *> allChangedObjects;
	allChangedObjects.Append(SelectedObjects);
	allChangedObjects.Append(LastSelectedObjectSet);

	UpdateRenderFlags(allChangedObjects);

	LastSelectedObjectSet.Reset();
	LastSelectedObjectSet.Append(SelectedObjects);
}

void AEditModelPlayerState_CPP::PostViewChanged()
{
	// Gather a set of objects whose render mode may have changed due to either
	// selection, hover, or view group hierarchy
	TSet<FModumateObjectInstance *> allChangedObjects;

	// Gather all objects previously and currently under the view group object
	ViewGroupDescendents.Reset();
	if (ViewGroupObject)
	{
		ViewGroupDescendents.Add(ViewGroupObject);
		ViewGroupDescendents.Append(ViewGroupObject->GetAllDescendents());
	}
	allChangedObjects.Append(ViewGroupDescendents);
	allChangedObjects.Append(LastViewGroupDescendentsSet);

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
	auto &doc = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	for (auto &kvp : ObjectErrorMap)
	{
		if (kvp.Value.Num() > 0)
		{
			if (auto *moi = doc.GetObjectById(kvp.Key))
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

	LastViewGroupDescendentsSet.Reset();
	LastViewGroupDescendentsSet.Append(ViewGroupDescendents);

	LastHoveredObjectSet.Reset();
	LastHoveredObjectSet.Append(HoveredObjectDescendents);

	LastErrorObjectSet.Reset();
	LastErrorObjectSet.Append(ErrorObjects);

	// Also, use this opportunity to iterate through the scene graph and cache the set of objects
	// underneath the current view group (if it's set), and are not underneath a group.
	// TODO: only call this when necessary (when ViewGroupObject or the object list/hierarchy changes).
	FindReachableObjects(LastReachableObjectSet);
}

void AEditModelPlayerState_CPP::OnNewModel()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::OnNewModel"));
	if (EMPlayerController->ToolIsInUse())
	{
		EMPlayerController->AbortUseTool();
	}
	EMPlayerController->SetToolMode(EToolMode::VE_SELECT);

	SnappedCursor.ClearAffordanceFrame();

	// Reset fields that have pointers/references to MOIs
	HoveredObject = nullptr;
	ViewGroupObject = nullptr;
	SelectedObjects.Reset();
	LastSelectedObjectSet.Reset();
	LastViewGroupDescendentsSet.Reset();
	LastHoveredObjectSet.Reset();
	LastErrorObjectSet.Reset();
	LastReachableObjectSet.Reset();
	ObjectErrorMap.Reset();
	LastFilePath.Empty();

	PostViewChanged();

	auto gameInstance = Cast<UModumateGameInstance>(GetGameInstance());
	gameInstance->DimensionManager->Reset();

	PostOnNewModel.Broadcast();
}

void AEditModelPlayerState_CPP::SetShowHoverEffects(bool showHoverEffects)
{
	ShowHoverEffects = showHoverEffects;
	PostViewChanged();
}

FModumateObjectInstance *AEditModelPlayerState_CPP::GetValidHoveredObjectInView(FModumateObjectInstance *hoverTarget) const
{
	FModumateObjectInstance *highestGroupUnderViewGroup = nullptr;
	FModumateObjectInstance *nextTarget = hoverTarget;
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

void AEditModelPlayerState_CPP::SetHoveredObject(FModumateObjectInstance *ob)
{
//	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SetHoveredObject"));
	if (HoveredObject != ob)
	{
		if (HoveredObject != nullptr)
		{
			HoveredObject->MouseHoverActor(EMPlayerController, false);
		}
		HoveredObject = ob;
		if (ob != nullptr)
		{
			ob->MouseHoverActor(EMPlayerController, true);
		}
		PostViewChanged();
	}
}

void AEditModelPlayerState_CPP::SetObjectSelected(FModumateObjectInstance *ob, bool selected)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SetObjectSelected"));

	if (selected)
	{
		SelectedObjects.Add(ob);
	}
	else
	{
		SelectedObjects.Remove(ob);
	}

	ob->OnSelected(selected);

	PostSelectionChanged();
	EMPlayerController->EditModelUserWidget->EMOnSelectionObjectChanged();
}

void AEditModelPlayerState_CPP::SetViewGroupObject(FModumateObjectInstance *ob)
{
	ViewGroupObject = ob;
	PostViewChanged();
}

bool AEditModelPlayerState_CPP::IsObjectInCurViewGroup(FModumateObjectInstance *obj) const
{
	if (ViewGroupObject)
	{
		return LastViewGroupDescendentsSet.Contains(obj);
	}

	return true;
}

FModumateObjectInstance *AEditModelPlayerState_CPP::FindHighestParentGroupInViewGroup(FModumateObjectInstance *obj) const
{
	if (!IsObjectInCurViewGroup(obj))
	{
		return nullptr;
	}

	FModumateObjectInstance *iter = obj;
	FModumateObjectInstance *highestGroup = iter;
	while (iter && (iter != ViewGroupObject))
	{
		if (iter->GetObjectType() == EObjectType::OTGroup)
		{
			highestGroup = iter;
		}

		iter = iter->GetParentObject();
	}

	return highestGroup;
}

void AEditModelPlayerState_CPP::FindReachableObjects(TSet<FModumateObjectInstance*> &reachableObjs) const
{
	reachableObjs.Reset();

	auto *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	auto &doc = gameState->Document;
	const auto &allObjects = doc.GetObjectInstances();
	int32 viewGroupObjID = ViewGroupObject ? ViewGroupObject->ID : 0;

	TQueue<FModumateObjectInstance *> objQueue;

	for (auto *obj : allObjects)
	{
		if ((obj->GetParentID() == viewGroupObjID) && obj->IsSelectableByUser())
		{
			objQueue.Enqueue(obj);
		}
	}

	TSet<FModumateObjectInstance *> visited;
	FModumateObjectInstance *iter = nullptr;

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

bool AEditModelPlayerState_CPP::IsObjectReachableInView(FModumateObjectInstance* obj) const
{
	return LastReachableObjectSet.Contains(obj);
}

void AEditModelPlayerState_CPP::GetSelectorModumateObjects(TArray<AActor*>& ModumateObjects)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::GetSelectorModumateObjects"));
	TArray<AActor*> objectRefs;
	for (FModumateObjectInstance*& curObjectRef : SelectedObjects)
	{
		if (curObjectRef->GetActor() != nullptr)
		{
			objectRefs.Add(curObjectRef->GetActor());
		}
	}
	ModumateObjects = objectRefs;
}

AActor* AEditModelPlayerState_CPP::GetViewGroupObject() const
{
	return ViewGroupObject ? ViewGroupObject->GetActor() : nullptr;
}

AActor* AEditModelPlayerState_CPP::GetHoveredObject() const
{
	return HoveredObject ? HoveredObject->GetActor() : nullptr;
}

bool AEditModelPlayerState_CPP::SetErrorForObject(int32 ObjectID, FName ErrorTag, bool bIsError)
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

bool AEditModelPlayerState_CPP::ClearErrorsForObject(int32 ObjectID)
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

bool AEditModelPlayerState_CPP::DoesObjectHaveError(int32 ObjectID, FName ErrorTag) const
{
	const TSet<FName> *objectErrors = ObjectErrorMap.Find(ObjectID);
	return (objectErrors && objectErrors->Contains(ErrorTag));
}

bool AEditModelPlayerState_CPP::DoesObjectHaveAnyError(int32 ObjectID) const
{
	const TSet<FName> *objectErrors = ObjectErrorMap.Find(ObjectID);
	return (objectErrors && (objectErrors->Num() > 0));
}

void AEditModelPlayerState_CPP::CopySelectedToClipboard(const FModumateDocument &document)
{
	ClipboardEntries.Reset();
	// TODO: re-implement
}

void AEditModelPlayerState_CPP::Paste(FModumateDocument &document) const
{
	if (ClipboardEntries.Num() == 0)
	{
		return;
	}
	document.BeginUndoRedoMacro();
	// TODO: re-implement
	document.EndUndoRedoMacro();
}

FBIMKey AEditModelPlayerState_CPP::GetAssemblyForToolMode(EToolMode mode)
{
	TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(mode);
	if (ensureAlways(tool))
	{
		return tool->GetAssemblyKey();
	}
	return FBIMKey();
}

void AEditModelPlayerState_CPP::SetAssemblyForToolMode(EToolMode Mode, const FBIMKey& AssemblyKey)
{
	TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(Mode);
	if (ensureAlways(tool))
	{
		tool->SetAssemblyKey(AssemblyKey);
	}
}

bool AEditModelPlayerState_CPP::SetEditMode(EEditViewModes NewEditViewMode, bool bForceUpdate)
{
	if (((SelectedViewMode != NewEditViewMode) || bForceUpdate) && EMPlayerController && EMPlayerController->ValidEditModes.Contains(NewEditViewMode))
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
			float metaPlaneAlphaValue = (SelectedViewMode == EEditViewModes::MetaPlanes) ? 1.0f : 0.75f;
			static const FName metaPlaneAlphaParamName(TEXT("MetaPlaneAlpha"));
			UMaterialParameterCollectionInstance* metaPlaneMatParamInst = GetWorld()->GetParameterCollectionInstance(MetaPlaneMatParamCollection);
			metaPlaneMatParamInst->SetScalarParameterValue(metaPlaneAlphaParamName, metaPlaneAlphaValue);
		}

		UpdateObjectVisibilityAndCollision();
		return true;
	}

	return false;
}

void AEditModelPlayerState_CPP::UpdateObjectVisibilityAndCollision()
{
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument &doc = gameState->Document;
	for (FModumateObjectInstance *moi : doc.GetObjectInstances())
	{
		if (moi)
		{
			moi->UpdateVisibilityAndCollision();
		}
	}
}

bool AEditModelPlayerState_CPP::IsObjectTypeEnabledByViewMode(EObjectType ObjectType) const
{
	switch (ObjectType)
	{
	case EObjectType::OTRoom:
		return SelectedViewMode == EEditViewModes::Rooms;
	case EObjectType::OTMetaVertex:
	case EObjectType::OTMetaEdge:
	case EObjectType::OTMetaPlane:
		return SelectedViewMode == EEditViewModes::MetaPlanes;
	case EObjectType::OTSurfaceGraph:
	case EObjectType::OTSurfaceVertex:
	case EObjectType::OTSurfaceEdge:
	case EObjectType::OTSurfacePolygon:
		return SelectedViewMode == EEditViewModes::SurfaceGraphs;
	case EObjectType::OTCutPlane:
	case EObjectType::OTScopeBox:
		return true;
	case EObjectType::OTWallSegment:
	case EObjectType::OTFloorSegment:
	case EObjectType::OTCeiling:
	case EObjectType::OTRoofFace:
	case EObjectType::OTRailSegment:
		return (SelectedViewMode == EEditViewModes::ObjectEditing) || (SelectedViewMode == EEditViewModes::SurfaceGraphs);
	default:
		return SelectedViewMode == EEditViewModes::ObjectEditing;
	}
}
