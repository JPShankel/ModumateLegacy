// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelPlayerState_CPP.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerHUD_CPP.h"
#include "LineActor.h"
#include "ModumateAnalyticsStatics.h"
#include "ModumateCommands.h"
#include "ModumateConsoleCommand.h"
#include "ModumateDimensionStatics.h"
#include "ModumateDocument.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateObjectDatabase.h"
#include "Algo/Transform.h"


using namespace Modumate;

AEditModelPlayerState_CPP::AEditModelPlayerState_CPP()
	: ShowDebugSnaps(false)
	, ShowDocumentDebug(false)
	, bShowGraphDebug(false)
	, bShowVolumeDebug(false)
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

	PostSelectionOrViewChanged();
	SetEditViewModeDirect(EEditViewModes::ObjectEditing, true);
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
		AddDimensionStringsToHUDDrawWidget();
	}
}

void AEditModelPlayerState_CPP::BatchRenderLines()
{
	CurSelectionStructurePoints.Reset();
	CurSelectionStructureLines.Reset();
	CurViewGroupObjects.Reset();

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument &doc = gameState->Document;

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
	AEditModelPlayerHUD_CPP *emPlayerHUD = Cast<AEditModelPlayerHUD_CPP>(EMPlayerController->GetHUD());
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
	if (EMPlayerController->bCutPlaneVisible)
	{
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
}

void AEditModelPlayerState_CPP::SetShowGraphDebug(bool bShow)
{
	if (bShowGraphDebug != bShow)
	{
		bShowGraphDebug = bShow;

		// TODO: replace with non-debug visibility flag for rooms; for now, just piggypack off of the room graph visibility
		AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
		Modumate::FModumateDocument &doc = gameState->Document;
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


void AEditModelPlayerState_CPP::SetAssemblyForActor(AActor *actor, const FShoppingItem &assembly)
{
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;
	FModumateObjectInstance *ob = doc->ObjectFromActor(actor);

	if (ob != nullptr)
	{
		// Search for the right assembly from the correct tool mode
		EToolMode searchInToolMode = UModumateTypeStatics::ToolModeFromObjectType(ob->GetObjectType());

		FName modeName = FindEnumValueFullName<EToolMode>(TEXT("EToolMode"), searchInToolMode);

		TArray<int32> ids;
		ids.Add(ob->ID);

		EMPlayerController->ModumateCommand(
			Modumate::FModumateCommand(Modumate::Commands::kSetAssemblyForObjects)
			.Param(Modumate::Parameters::kObjectIDs, ids)
			.Param(Modumate::Parameters::kToolMode,modeName.ToString())
			.Param(Modumate::Parameters::kAssembly, assembly.Key));
	}
}

void AEditModelPlayerState_CPP::ToggleRoomViewMode()
{
	if (SelectedViewMode == EEditViewModes::Rooms)
	{
		SetEditViewModeCommand(PreviousModeFromToggleRoomView);
	}
	else
	{
		PreviousModeFromToggleRoomView = SelectedViewMode;
		SetEditViewModeCommand(EEditViewModes::Rooms);
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

FModumateObjectAssembly AEditModelPlayerState_CPP::SetTemporaryAssembly(
	EToolMode mode,
	const FModumateObjectAssembly &assembly)
{
	FModumateObjectAssembly newTemp = assembly;
	newTemp.DatabaseKey = TEXT("temp");
	TemporaryAssemblies.Add(mode, newTemp);
	return newTemp;
}

const FModumateObjectAssembly *AEditModelPlayerState_CPP::GetTemporaryAssembly(EToolMode mode) const
{
	return TemporaryAssemblies.Find(mode);
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

	bool selectionOrViewChanged = false;

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;

	for (int32 i = SelectedObjects.Num() - 1; i >= 0; --i)
	{
		FModumateObjectInstance *selectedObj = SelectedObjects[i];
		if ((selectedObj == nullptr) || selectedObj->IsDestroyed())
		{
			SelectedObjects.RemoveAt(i);
			selectionOrViewChanged = true;
		}
	}

	auto *nextViewGroup = ViewGroupObject;
	while (nextViewGroup && nextViewGroup->IsDestroyed())
	{
		nextViewGroup = nextViewGroup->GetParentObject();
		selectionOrViewChanged = true;
	}

	TArray<int32> invalidErrorIDs;
	for (auto &kvp : ObjectErrorMap)
	{
		int32 objID = kvp.Key;
		FModumateObjectInstance *obj = doc->GetObjectById(objID);
		if ((obj == nullptr) || obj->IsDestroyed())
		{
			invalidErrorIDs.Add(objID);
			selectionOrViewChanged = true;
		}
	}
	for (int32 invalidObjID : invalidErrorIDs)
	{
		ObjectErrorMap.Remove(invalidObjID);
	}

	if (HoveredObject && HoveredObject->IsDestroyed())
	{
		HoveredObject = nullptr;
		selectionOrViewChanged = true;
	}

	if (selectionOrViewChanged)
	{
		PostSelectionOrViewChanged();
	}

	return selectionOrViewChanged;
}

void AEditModelPlayerState_CPP::SelectAll()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SelectAll"));

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;

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

	PostSelectionOrViewChanged();
	OnSelectionObjectChanged.Broadcast();
}

void AEditModelPlayerState_CPP::SelectInverse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SelectInverse"));

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;

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

	PostSelectionOrViewChanged();
	OnSelectionObjectChanged.Broadcast();
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
	Modumate::FModumateDocument *doc = &gameState->Document;
	auto obs = doc->GetObjectInstances();

	for (auto &ob : obs)
	{
		ob->OnSelected(false);
	}

	SelectedObjects.Empty();

	PostSelectionOrViewChanged();
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

void AEditModelPlayerState_CPP::PostSelectionOrViewChanged()
{
	// Gather a set of objects whose render mode may have changed due to either
	// selection, hover, or view group hierarchy
	TSet<FModumateObjectInstance *> allChangedObjects;

	// Gather all previously selected and currently selected objects
	TSet<FModumateObjectInstance *> selectedObjectSet(SelectedObjects);
	allChangedObjects.Append(selectedObjectSet);
	allChangedObjects.Append(LastSelectedObjectSet);

	// Gather all objects previously and currently under the view group object
	TSet<FModumateObjectInstance *> viewGroupDescendentsSet;
	if (ViewGroupObject)
	{
		viewGroupDescendentsSet.Add(ViewGroupObject);
		viewGroupDescendentsSet.Append(ViewGroupObject->GetAllDescendents());
	}
	allChangedObjects.Append(viewGroupDescendentsSet);
	allChangedObjects.Append(LastViewGroupDescendentsSet);

	// Gather all objects previously and currently hovered objects
	TSet<FModumateObjectInstance *> hoveredObjectsSet;
	if (HoveredObject)
	{
		hoveredObjectsSet.Add(HoveredObject);
		hoveredObjectsSet.Append(HoveredObject->GetAllDescendents());
	}
	allChangedObjects.Append(hoveredObjectsSet);
	allChangedObjects.Append(LastHoveredObjectSet);

	// Gather all objects previously and currently having errors
	auto &doc = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	TSet<FModumateObjectInstance *> errorObjectSet;
	for (auto &kvp : ObjectErrorMap)
	{
		if (kvp.Value.Num() > 0)
		{
			if (auto *moi = doc.GetObjectById(kvp.Key))
			{
				errorObjectSet.Add(moi);
			}
		}
	}
	allChangedObjects.Append(errorObjectSet);
	allChangedObjects.Append(LastErrorObjectSet);

	// Iterate through all objects whose render mode may have changed due to either selection or view group hierarchy
	TArray<TWeakObjectPtr<AAdjustmentHandleActor_CPP>> adjustHandleActors;
	for (FModumateObjectInstance *moi : allChangedObjects)
	{
		if (moi && !moi->IsDestroyed())
		{
			bool bHovered = (ShowHoverEffects && hoveredObjectsSet.Contains(moi));

			int32 selectionValue = selectedObjectSet.Contains(moi) ? 0x1 : 0x0;
			int32 viewGroupValue = viewGroupDescendentsSet.Contains(moi) ? 0x2 : 0x0;
			int32 hoverValue = bHovered ? 0x4 : 0x0;
			int32 errorValue = errorObjectSet.Contains(moi) ? 0x8 : 0x0;
			int32 stencilValue = selectionValue | viewGroupValue | hoverValue | errorValue;

			SetActorRenderValues(moi->GetActor(), stencilValue, bHovered);
			moi->GetAdjustmentHandleActors(adjustHandleActors);
			for (auto& adjustHandleActor : adjustHandleActors)
			{
				SetActorRenderValues(adjustHandleActor.Get(), stencilValue, bHovered);
			}
		}
	}

	OnSelectionChanged.Broadcast();

	// Reset dimension string tab state
	if (EMPlayerController)
	{
		EMPlayerController->ResetDimensionStringTabState();
	}

	LastSelectedObjectSet.Reset();
	LastSelectedObjectSet.Append(selectedObjectSet);

	LastViewGroupDescendentsSet.Reset();
	LastViewGroupDescendentsSet.Append(viewGroupDescendentsSet);

	LastHoveredObjectSet.Reset();
	LastHoveredObjectSet.Append(hoveredObjectsSet);

	LastErrorObjectSet.Reset();
	LastErrorObjectSet.Append(errorObjectSet);

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

	PostSelectionOrViewChanged();
	PostOnNewModel.Broadcast();
}

void AEditModelPlayerState_CPP::SetShowHoverEffects(bool showHoverEffects)
{
	ShowHoverEffects = showHoverEffects;
	PostSelectionOrViewChanged();
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
		PostSelectionOrViewChanged();
	}
}

void AEditModelPlayerState_CPP::SetObjectSelected(FModumateObjectInstance *ob, bool selected)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SetObjectSelected"));

	if (selected)
	{
		SelectedObjects.AddUnique(ob);
	}
	else
	{
		SelectedObjects.Remove(ob);
	}

	ob->OnSelected(selected);

	PostSelectionOrViewChanged();
	OnSelectionObjectChanged.Broadcast();
}

void AEditModelPlayerState_CPP::SetViewGroupObject(FModumateObjectInstance *ob)
{
	ViewGroupObject = ob;
	PostSelectionOrViewChanged();
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

bool AEditModelPlayerState_CPP::SetObjectHeight(FModumateObjectInstance *obj, float newHeight, bool bSetBaseElevation, bool bUpdateGeometry)
{
	static const float minHeight = 0.1f;
	static const FName invalidHeightErrorName(TEXT("InvalidHeight"));

	if (obj && (obj->GetControlPoints().Num() > 0))
	{
		float oldBaseElevation = obj->GetControlPoint(0).Z;
		float newBaseElevation = oldBaseElevation;
		float oldHeight = obj->GetExtents().Y;
		if (bSetBaseElevation)
		{
			newBaseElevation = newHeight;
			newHeight = oldBaseElevation + oldHeight - newBaseElevation;
		}

		bool bValidHeight = (newHeight >= minHeight);
		newHeight = FMath::Max(newHeight, minHeight);
		if (bSetBaseElevation)
		{
			newBaseElevation = oldBaseElevation + oldHeight - newHeight;
		}

		if (ADynamicMeshActor *meshActor = Cast<ADynamicMeshActor>(obj->GetActor()))
		{
			meshActor->SetPlacementError(invalidHeightErrorName, !bValidHeight);
		}

		if (bSetBaseElevation)
		{
			for (int32 i=0;i<obj->GetControlPoints().Num();++i)
			{
				FVector CP = obj->GetControlPoint(i);
				CP.Z = newBaseElevation;
				obj->SetControlPoint(i, CP);
			}
		}

		FVector extents = obj->GetExtents();
		extents.Y = newHeight;
		obj->SetExtents(extents);

		if (bUpdateGeometry)
		{
			obj->UpdateGeometry();
		}

		return bValidHeight;
	}

	return false;
}

bool AEditModelPlayerState_CPP::SetSelectedHeight(float HeightEntry, bool bIsDelta, bool bUpdateGeometry)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SetSelectedHeight"));

	bool bAnySuccess = false;
	int32 numObjects = SelectedObjects.Num();
	for (int32 i = 0; i < numObjects; i++)
	{
		if (SelectedObjects[i]->GetObjectType() == EObjectType::OTWallSegment)
		{
			FModumateObjectInstance *obj = SelectedObjects[i];
			float newHeight = HeightEntry;
			if (bIsDelta) // height implied as an increment to current height
			{
				newHeight = obj->GetExtents()[1] + HeightEntry;
			}

			bool bSuccess = SetObjectHeight(obj, newHeight, false, bUpdateGeometry);
			bAnySuccess = bAnySuccess || bSuccess;
		}
	}

	OnHeightUpdate();

	return bAnySuccess;
}

bool AEditModelPlayerState_CPP::SetSelectedControlPointsHeight(float newCPHeight, bool bIsDelta, bool bUpdateGeometry)
{
	UE_LOG(LogCallTrace, Display, TEXT("AEditModelPlayerState_CPP::SetSelectedControlPointsFreezeHeight"));

	bool bAnySuccess = false;
	int32 numObjects = SelectedObjects.Num();
	for (int32 i = 0; i < numObjects; i++)
	{
		if (SelectedObjects[i]->GetObjectType() == EObjectType::OTWallSegment)
		{
			FModumateObjectInstance *obj = SelectedObjects[i];

			bool bSuccess = SetObjectHeight(obj, newCPHeight, true, bUpdateGeometry);
			bAnySuccess = bAnySuccess || bSuccess;
		}
	}

	return bAnySuccess;
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
		PostSelectionOrViewChanged();
	}

	return bChanged;
}

bool AEditModelPlayerState_CPP::ClearErrorsForObject(int32 ObjectID)
{
	TSet<FName> *objectErrors = ObjectErrorMap.Find(ObjectID);

	if (objectErrors && (objectErrors->Num() > 0))
	{
		objectErrors->Reset();
		PostSelectionOrViewChanged();
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

bool AEditModelPlayerState_CPP::AnySelectedPlacementErrors() const
{
	for (const FModumateObjectInstance *selectedObj : SelectedObjects)
	{
		const ADynamicMeshActor *meshActor = Cast<ADynamicMeshActor>(selectedObj->GetActor());
		if (meshActor && meshActor->HasPlacementError())
		{
			return true;
		}
	}

	return false;
}

void AEditModelPlayerState_CPP::ClearSelectedPlacementErrorsAndHandles()
{
	for (FModumateObjectInstance *selectedObj : SelectedObjects)
	{
		if (ADynamicMeshActor *meshActor = Cast<ADynamicMeshActor>(selectedObj->GetActor()))
		{
			meshActor->ClearPlacementErrors();
			meshActor->AdjustHandleSide = -1;
		}
	}
}

void AEditModelPlayerState_CPP::CopySelectedToClipboard(const Modumate::FModumateDocument &document)
{
	ClipboardEntries.Reset();
	Algo::Transform(
		SelectedObjects,
		ClipboardEntries,
		[](FModumateObjectInstance *ob)
		{
			FMOIDataRecord ret = ob->AsDataRecord();
			ret.ID = 0;
			return ret;
		});
}

void AEditModelPlayerState_CPP::Paste(Modumate::FModumateDocument &document) const
{
	if (ClipboardEntries.Num() == 0)
	{
		return;
	}
	document.BeginUndoRedoMacro();
	for (auto ce : ClipboardEntries)
	{
		document.CreateObjectFromRecord(GetWorld(), ce);
	}
	document.EndUndoRedoMacro();
}

const FShoppingItem &AEditModelPlayerState_CPP::GetAssemblyForToolMode(EToolMode mode)
{
	TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(mode);
	if (ensureAlways(tool))
	{
		return tool->GetAssembly();
	}

	return FShoppingItem::ErrorItem;
}

void AEditModelPlayerState_CPP::SetAssemblyForToolMode(EToolMode mode, const FShoppingItem &item)
{
	TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(mode);
	if (ensureAlways(tool))
	{
		tool->SetAssembly(item);
	}
}

TArray<FModelDimensionString> AEditModelPlayerState_CPP::GetDimensionStringFromGroupID(const FName &groupID, int32 &maxIndexInGroup)
{
	TArray<FModelDimensionString> returnDimString;
	int32 returnMaxIndex = 0;
	for (auto curDimStirng : DimensionStrings)
	{
		if (curDimStirng.GroupID == groupID)
		{
			returnMaxIndex = FMath::Max(returnMaxIndex, curDimStirng.GroupIndex);
			returnDimString.Add(curDimStirng);
		}
	}
	maxIndexInGroup = returnMaxIndex;
	return returnDimString;
}

void AEditModelPlayerState_CPP::AddDimensionStringsToHUDDrawWidget()
{
	if (DimensionStrings.Num() == 0)
	{
		return;
	}

	TArray<FName> dimStringGroupIDs;

	for (auto curDim : DimensionStrings)
	{
		dimStringGroupIDs.AddUnique(curDim.GroupID);
	}
	if (dimStringGroupIDs.Num() == 0)
	{
		DimensionStrings.Reset();
		return;
	}
	// Draw dimension strings based on group
	// "Default" is prioritize first, follow by playerController.
	// If none of the above, controller's DimStringWidgetSelectedObject. Ex: portal width and height dim string
	// TODO: if Enterablefield widget is selected, it should contain a groupID to be used, instead of using DimStringWidgetSelectedObject's name
	FName currentDimensionStringGroupID = "Default";
	if (dimStringGroupIDs.Contains("PlayerController"))
	{
		currentDimensionStringGroupID = "PlayerController";
	}
	else
	{
		if (EMPlayerController->DimStringWidgetSelectedObject)
		{
			currentDimensionStringGroupID = EMPlayerController->DimStringWidgetSelectedObject->GetFName();
		}
		else
		{
			currentDimensionStringGroupID = dimStringGroupIDs[0];
		}
	}

	// Find the max. index within the current groupID
	// TODO: use dimension string uniqueID to determine the max index
	CurrentDimensionStringGroupIndexMax = 0;
	for (auto curDim : DimensionStrings)
	{
		if (curDim.GroupID == currentDimensionStringGroupID)
		{
			CurrentDimensionStringGroupIndexMax = FMath::Max(CurrentDimensionStringGroupIndexMax, curDim.GroupIndex);
		}
	}
	// Only draw dimension strings that have groupID, and the correct current index
	// TODO: use uniqueID to determine TAB switch state instead of using index
	TArray<FModelDimensionString> dimStringsToDraw;
	for (auto curDim : DimensionStrings)
	{
		if (curDim.bAlwaysVisible)
		{
			dimStringsToDraw.Add(curDim);
		}
		else if((curDim.GroupID == currentDimensionStringGroupID) &&
			curDim.GroupIndex == CurrentDimensionStringGroupIndex)
		{
			dimStringsToDraw.Add(curDim);
		}
	}

	for (auto curDim : dimStringsToDraw)
	{
		TArray<FModumateLines> convertedLines = ConvertDimensionStringsToModumateLines(curDim);
		if (convertedLines.Num() > 0)
		{
			EMPlayerController->HUDDrawWidget->LinesToDraw.Append(convertedLines);
		}
		if (curDim.Functionality == EEnterableField::EditableText_ImperialUnits_UserInput)
		{
			CurrentDimensionStringWithInputUniqueID = curDim.UniqueID;
		}
	}

	DimensionStrings.Reset();
}

TArray<FModumateLines> AEditModelPlayerState_CPP::ConvertDimensionStringsToModumateLines(FModelDimensionString dimString)
{
	TArray<FModumateLines> finalLines;
	if (dimString.Owner == nullptr)
	{
		return finalLines;
	}

	if (dimString.Style == EDimStringStyle::DegreeString)
	{
		EMPlayerController->HUDDrawWidget->DegreeStringToDraw.Add(dimString);
		return finalLines;
	}

	// Amount of offset for the main dim line
	float offsetWorldDistance = dimString.Offset;

	FVector cameraLoc = EMPlayerController->PlayerCameraManager->GetCameraLocation();
	float cameraFOV = EMPlayerController->PlayerCameraManager->GetFOVAngle();

	// Let camera angle determines direction of offset
	if (dimString.Style == EDimStringStyle::HCamera)
	{
		FVector halfPoint = (dimString.Point1 + dimString.Point2) / 2.f;
		float distPoint1 = ((halfPoint + dimString.OffsetDirection) - cameraLoc).Size();
		float distPoint2 = ((halfPoint + dimString.OffsetDirection * -1.f) - cameraLoc).Size();

		if (distPoint1 > distPoint2)
		{
			offsetWorldDistance = offsetWorldDistance * -1.f;
		}
	}

	// Determine start and end of main line
	FVector point1Offset = dimString.Point1 + (dimString.OffsetDirection * offsetWorldDistance);
	FVector point2Offset = dimString.Point2 + (dimString.OffsetDirection * offsetWorldDistance);
	FModumateImperialUnits imperialUnits = UModumateDimensionStatics::CentimeterToImperial((dimString.Point1 - dimString.Point2).Size());
	FVector mainLineTextLoc = (point1Offset + point2Offset) / 2.f;

	FModumateLines newMainLine;
	newMainLine.Point1 = point1Offset;
	newMainLine.Point2 = point2Offset;
	newMainLine.Color = dimString.Color;
	newMainLine.OutStrings = { imperialUnits.FeetWhole, imperialUnits.InchesWhole, imperialUnits.InchesNumerator, imperialUnits.InchesDenominator };
	newMainLine.TextLocation = mainLineTextLoc;
	newMainLine.OwnerActor = dimString.Owner;
	newMainLine.Functionality = dimString.Functionality;
	newMainLine.DraftAestheticType = 2;
	newMainLine.UniqueID = dimString.UniqueID;

	finalLines.Add(newMainLine);

	if (dimString.Style == EDimStringStyle::RotateToolLine) // Line is Drawn with degree from rotation tool
	{
		// Get scale of degree plane
		float sizeApprox = (cameraLoc - dimString.Point1).Size() * (FMath::Sin(PI / (180.f) * cameraFOV)) / 800.f;
		float sizeX = sizeApprox * -1.f;

		FVector degreePlaneSize = FVector(sizeX, sizeApprox, 1.f);
		FTransform degreePlaneTransform = FTransform(FRotator::ZeroRotator, dimString.Point1, degreePlaneSize);
		FVector approxLoc = degreePlaneTransform.TransformPosition(FVector(40.f, 0.f, 0.f));
		float approxLength = (dimString.Point1 - approxLoc).Size();

		// Vertical tick line
		FVector tickDirection = (dimString.Point2 - dimString.Point1).GetSafeNormal();
		FVector tickPlacement = (tickDirection * approxLength) + dimString.Point1;

		FModumateLines tickLineVertical;
		tickLineVertical.Point1 = tickPlacement + FVector(0.f, 0.f, -0.5f);
		tickLineVertical.Point2 = tickPlacement + FVector(0.f, 0.f, 0.5f);
		tickLineVertical.DraftAestheticType = 2;
		tickLineVertical.UniqueID = dimString.UniqueID;
		tickLineVertical.OwnerActor = dimString.Owner;
		tickLineVertical.Color = dimString.Color;

		// Horizontal tick line
		FVector tickHorizontalAxis = tickDirection.RotateAngleAxis(90.f, FVector::UpVector);

		FModumateLines tickLineHorizontal;
		tickLineHorizontal.Point1 = tickPlacement + (tickHorizontalAxis * 0.5f);
		tickLineHorizontal.Point2 = tickPlacement + (tickHorizontalAxis * -0.5f);
		tickLineHorizontal.DraftAestheticType = 2;
		tickLineHorizontal.UniqueID = dimString.UniqueID;
		tickLineHorizontal.OwnerActor = dimString.Owner;
		tickLineHorizontal.Color = dimString.Color;

		finalLines.Add(tickLineVertical);
		finalLines.Add(tickLineHorizontal);
	}
	else
	{
		// Offset lines: Draw lines that connect the main line back to its original points
		FModumateLines offsetLine1;
		offsetLine1.Point1 = dimString.Point1;
		offsetLine1.Point2 = point1Offset;
		offsetLine1.Color = dimString.Color;
		offsetLine1.DraftAestheticType = 1;
		offsetLine1.UniqueID = dimString.UniqueID;
		finalLines.Add(offsetLine1);

		FModumateLines offsetLine2;
		offsetLine2.Point1 = dimString.Point2;
		offsetLine2.Point2 = point2Offset;
		offsetLine2.Color = dimString.Color;
		offsetLine2.DraftAestheticType = 1;
		offsetLine2.UniqueID = dimString.UniqueID;
		finalLines.Add(offsetLine2);

		// Perpendicular Lines: Draw lines that perpendicular to both main and offset line, ex: vertical line of wall dim. string
		FVector perpAxis1 = (point1Offset - point2Offset).GetSafeNormal();
		FVector perpVect1 = (point1Offset - dimString.Point1).GetSafeNormal();
		FVector rotatedVect1 = perpVect1.RotateAngleAxis(90.f, perpAxis1.GetSafeNormal());
		FModumateLines perpLine1;
		perpLine1.Point1 = point1Offset + (rotatedVect1 * 0.25f);
		perpLine1.Point2 = point1Offset + (rotatedVect1 * -0.25f);
		perpLine1.Color = dimString.Color;
		perpLine1.DraftAestheticType = 2;
		perpLine1.UniqueID = dimString.UniqueID;
		finalLines.Add(perpLine1);

		FVector perpAxis2 = (point2Offset - point1Offset).GetSafeNormal();
		FVector perpVect2 = (point2Offset - dimString.Point2).GetSafeNormal();
		FVector rotatedVect2 = perpVect2.RotateAngleAxis(90.f, perpAxis2.GetSafeNormal());
		FModumateLines perpLine2;
		perpLine2.Point1 = point2Offset + (rotatedVect2 * 0.25f);
		perpLine2.Point2 = point2Offset + (rotatedVect2 * -0.25f);
		perpLine2.Color = dimString.Color;
		perpLine2.DraftAestheticType = 2;
		perpLine2.UniqueID = dimString.UniqueID;
		finalLines.Add(perpLine2);
	}

	return finalLines;
}

void AEditModelPlayerState_CPP::SetEditViewModeDirect(EEditViewModes NewEditViewMode, bool bForceUpdate)
{
	if ((SelectedViewMode != NewEditViewMode) || bForceUpdate)
	{
		SelectedViewMode = NewEditViewMode;
		EMPlayerController->UpdateMouseTraceParams();

		// Update global meta plane material params
		if (MetaPlaneMatParamCollection)
		{
			float metaPlaneAlphaValue = (SelectedViewMode == EEditViewModes::MetaPlanes) ? 1.0f : 0.75f;
			static const FName metaPlaneAlphaParamName(TEXT("MetaPlaneAlpha"));
			UMaterialParameterCollectionInstance* metaPlaneMatParamInst = GetWorld()->GetParameterCollectionInstance(MetaPlaneMatParamCollection);
			metaPlaneMatParamInst->SetScalarParameterValue(metaPlaneAlphaParamName, metaPlaneAlphaValue);
		}

		UpdateObjectVisibilityAndCollision();
	}
}

void AEditModelPlayerState_CPP::SetEditViewModeCommand(EEditViewModes NewEditViewMode)
{
	EMPlayerController->ModumateCommand(
		Modumate::FModumateCommand(Modumate::Commands::kSetEditViewMode, true)
		.Param(Modumate::Parameters::kEditViewMode, EnumValueString(EEditViewModes, NewEditViewMode)));
}

void AEditModelPlayerState_CPP::UpdateObjectVisibilityAndCollision()
{
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument &doc = gameState->Document;
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
	case EObjectType::OTCutPlane:
	case EObjectType::OTScopeBox: // TODO: cut planes and scope boxes will have individually toggled visibility in the app
		return EMPlayerController->bCutPlaneVisible;
	default:
		return SelectedViewMode == EEditViewModes::ObjectEditing;
	}
}
