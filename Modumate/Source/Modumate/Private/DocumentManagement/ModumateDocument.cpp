// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateDocument.h"

#include "Algo/Accumulate.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/DocumentDelta.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/ModumateDraftingView.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph3DDelta.h"
#include "Internationalization/Internationalization.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateMitering.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/PlatformFunctions.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "Objects/MOIFactory.h"
#include "Objects/SurfaceGraph.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/Modumate.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/ModumateObjectComponent.h"
#include "UnrealClasses/DynamicIconGenerator.h"

using namespace Modumate::Mitering;
using namespace Modumate;


const FName UModumateDocument::DocumentHideRequestTag(TEXT("DocumentHide"));

UModumateDocument::UModumateDocument(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NextID(1)
	, PrePreviewNextID(1)
	, ReservingObjectID(MOD_ID_NONE)
	, bApplyingPreviewDeltas(false)
	, bFastClearingPreviewDeltas(false)
	, bSlowClearingPreviewDeltas(false)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ModumateDocument"));

	//Set Default Values
	//Values will be deprecated in favor of instance-level overrides for assemblies
	DefaultWallHeight = 243.84f;
	ElevationDelta = 0;
	DefaultWindowHeight = 91.44f;
	DefaultDoorHeight = 0.f;
}

void UModumateDocument::PerformUndoRedo(UWorld* World, TArray<TSharedPtr<UndoRedo>>& FromBuffer, TArray<TSharedPtr<UndoRedo>>& ToBuffer)
{
	if (FromBuffer.Num() > 0)
	{
		TSharedPtr <UndoRedo> ur = FromBuffer.Last(0);
		FromBuffer.RemoveAt(FromBuffer.Num() - 1);

		TArray<FDeltaPtr> fromDeltas = ur->Deltas;
		Algo::Reverse(fromDeltas);

		ur->Deltas.Empty();
		Algo::Transform(fromDeltas, ur->Deltas, [](const FDeltaPtr& DeltaPtr) {return DeltaPtr->MakeInverse(); });
		Algo::ForEach(ur->Deltas, [this, World](FDeltaPtr& DeltaPtr) {DeltaPtr->ApplyTo(this, World); });

		ToBuffer.Add(ur);

#if WITH_EDITOR
		int32 fromBufferSize = FromBuffer.Num();
		int32 toBufferSize = ToBuffer.Num();
#endif

		CleanObjects(nullptr);
		PostApplyDeltas(World);
		UpdateRoomAnalysis(World);

		AEditModelPlayerState* EMPlayerState = Cast<AEditModelPlayerState>(World->GetFirstPlayerController()->PlayerState);
		EMPlayerState->RefreshActiveAssembly();

#if WITH_EDITOR
		ensureAlways(fromBufferSize == FromBuffer.Num());
		ensureAlways(toBufferSize == ToBuffer.Num());
#endif
	}
}

void UModumateDocument::Undo(UWorld *World)
{
	if (ensureAlways(!InUndoRedoMacro()))
	{
		PerformUndoRedo(World, UndoBuffer, RedoBuffer);
	}
}

void UModumateDocument::Redo(UWorld *World)
{
	if (ensureAlways(!InUndoRedoMacro()))
	{
		PerformUndoRedo(World, RedoBuffer, UndoBuffer);
	}
}

void UModumateDocument::BeginUndoRedoMacro()
{
	UndoRedoMacroStack.Push(UndoBuffer.Num());
}

void UModumateDocument::EndUndoRedoMacro()
{
	if (!InUndoRedoMacro())
	{
		return;
	}

	int32 start = UndoRedoMacroStack.Pop();

	if (start >= UndoBuffer.Num())
	{
		return;
	}

	TArray<TSharedPtr<UndoRedo>> section;
	for (int32 i = start; i < UndoBuffer.Num(); ++i)
	{
		section.Add(UndoBuffer[i]);
	}

	TSharedPtr<UndoRedo> ur = MakeShared<UndoRedo>();
	for (auto s : section)
	{
		ur->Deltas.Append(s->Deltas);
	}

	UndoBuffer.SetNum(start, true);

	UndoBuffer.Add(ur);
}

bool UModumateDocument::InUndoRedoMacro() const
{
	return (UndoRedoMacroStack.Num() > 0);
}

void UModumateDocument::SetDefaultWallHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWallHeight"));
	ClearRedoBuffer();
	float orig = DefaultWallHeight;
	DefaultWallHeight = height;
}

void UModumateDocument::SetDefaultRailHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight"));
	ClearRedoBuffer();
	float orig = DefaultRailHeight;
	DefaultRailHeight = height;
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight::Redo"));
}

void UModumateDocument::SetDefaultCabinetHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultCabinetHeight"));
	ClearRedoBuffer();
	float orig = DefaultCabinetHeight;
	DefaultCabinetHeight = height;
}

void UModumateDocument::SetDefaultDoorHeightWidth(float height, float width)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultDoorHeightWidth"));
	ClearRedoBuffer();
	float origHeight = DefaultDoorHeight;
	float origWidth = DefaultDoorWidth;
	DefaultDoorHeight = height;
	DefaultDoorWidth = width;
}

void UModumateDocument::SetDefaultWindowHeightWidth(float height, float width)
{
	float origHeight = DefaultWindowHeight;
	float origWidth = DefaultWindowWidth;
	DefaultWindowHeight = height;
	DefaultWindowWidth = width;
}

void UModumateDocument::SetDefaultJustificationZ(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationZ"));
	ClearRedoBuffer();
	float orig = DefaultJustificationZ;
	DefaultJustificationZ = newValue;
}

void UModumateDocument::SetDefaultJustificationXY(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationXY"));
	ClearRedoBuffer();
	float orig = DefaultJustificationXY;
	DefaultJustificationXY = newValue;
}

void UModumateDocument::SetAssemblyForObjects(UWorld *world,TArray<int32> ids, const FBIMAssemblySpec &assembly)
{
	if (ids.Num() == 0)
	{
		return;
	}

	auto delta = MakeShared<FMOIDelta>();
	for (auto id : ids)
	{
		AModumateObjectInstance* obj = GetObjectById(id);
		if (obj != nullptr)
		{
			auto& newState = delta->AddMutationState(obj);
			newState.AssemblyGUID = assembly.UniqueKey();
		}
	}

	ApplyDeltas({ delta }, world);
}

void UModumateDocument::AddHideObjectsById(UWorld *world, const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AddHideObjectsById"));

	for (auto id : ids)
	{
		AModumateObjectInstance *obj = GetObjectById(id);
		if (obj && !HiddenObjectsID.Contains(id))
		{
			obj->RequestHidden(UModumateDocument::DocumentHideRequestTag, true);
			obj->RequestCollisionDisabled(UModumateDocument::DocumentHideRequestTag, true);
			HiddenObjectsID.Add(id);
		}
	}
}

void UModumateDocument::UnhideAllObjects(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnhideAllObjects"));

	TSet<int32> ids = HiddenObjectsID;
	TSet<int32> hiddenCutPlaneIds;

	for (auto id : ids)
	{
		if (AModumateObjectInstance *obj = GetObjectById(id))
		{
			if (obj->GetObjectType() != EObjectType::OTCutPlane)
			{
				obj->RequestHidden(UModumateDocument::DocumentHideRequestTag, false);
				obj->RequestCollisionDisabled(UModumateDocument::DocumentHideRequestTag, false);
			}
			else
			{
				hiddenCutPlaneIds.Add(id);
			}
		}
	}

	HiddenObjectsID = hiddenCutPlaneIds;
	// TODO: Pending removal
	for (AModumateObjectInstance *obj : ObjectInstanceArray)
	{
		obj->UpdateVisuals();
	}
}

void UModumateDocument::UnhideObjectsById(UWorld *world, const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnhideObjectsById"));

	for (auto id : ids)
	{
		AModumateObjectInstance *obj = GetObjectById(id);
		if (obj && HiddenObjectsID.Contains(id))
		{
			obj->RequestHidden(UModumateDocument::DocumentHideRequestTag, false);
			obj->RequestCollisionDisabled(UModumateDocument::DocumentHideRequestTag, false);
			HiddenObjectsID.Remove(id);
		}
	}
	// TODO: Pending removal
	for (AModumateObjectInstance *obj : ObjectInstanceArray)
	{
		obj->UpdateVisuals();
	}
}

void UModumateDocument::RestoreDeletedObjects(const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RestoreDeletedObjects"));
	if (ids.Num() == 0)
	{
		return;
	}

	ClearRedoBuffer();
	TArray<AModumateObjectInstance*> oldObs;
	for (auto id : ids)
	{
		AModumateObjectInstance *oldOb = TryGetDeletedObject(id);
		if (oldOb != nullptr)
		{
			RestoreObjectImpl(oldOb);
			oldObs.AddUnique(oldOb);
		}
	}
}

bool UModumateDocument::DeleteObjectImpl(AModumateObjectInstance *ObjToDelete)
{
	if (ObjToDelete && !ObjToDelete->IsDestroyed())
	{
		// Store off the connected objects, in case they will be affected by this deletion
		TArray<AModumateObjectInstance *> connectedMOIs;
		ObjToDelete->GetConnectedMOIs(connectedMOIs);
		int32 objID = ObjToDelete->ID;

		bool bKeepDeletedObj = !bSlowClearingPreviewDeltas;
		bool bFullyDestroy = !bFastClearingPreviewDeltas && !bApplyingPreviewDeltas;

		if (bKeepDeletedObj)
		{
			// If we already had an object of the same ID in the deleted list, then we want to preserve it less than the current object being deleted.
			if (DeletedObjects.Contains(ObjToDelete->ID))
			{
				ensure(bFastClearingPreviewDeltas || bSlowClearingPreviewDeltas);
				DeletedObjects.FindAndRemoveChecked(ObjToDelete->ID)->Destroy();
			}

			ObjToDelete->DestroyMOI(bFullyDestroy);
			DeletedObjects.Add(ObjToDelete->ID, ObjToDelete);
		}
		else
		{
			ObjToDelete->Destroy();
		}

		ObjectInstanceArray.Remove(ObjToDelete);
		ObjectsByID.Remove(objID);

		// Update mitering, visibility & collision enabled on neighbors, in case they were dependent on this MOI.
		for (AModumateObjectInstance *connectedMOI : connectedMOIs)
		{
			connectedMOI->MarkDirty(EObjectDirtyFlags::Mitering | EObjectDirtyFlags::Visuals);
		}

		if (bTrackingDeltaObjects)
		{
			DeltaCreatedObjects.FindOrAdd(ObjToDelete->GetObjectType()).Remove(ObjToDelete->ID);
			DeltaDestroyedObjects.FindOrAdd(ObjToDelete->GetObjectType()).Add(ObjToDelete->ID);
		}

		return true;
	}

	return false;
}

bool UModumateDocument::RestoreObjectImpl(AModumateObjectInstance *obj)
{
	if (obj && (DeletedObjects.FindRef(obj->ID) == obj))
	{
		DeletedObjects.Remove(obj->ID);
		ObjectInstanceArray.AddUnique(obj);
		ObjectsByID.Add(obj->ID, obj);
		obj->RestoreMOI();

		return true;
	}

	return false;
}

AModumateObjectInstance* UModumateDocument::CreateOrRestoreObj(UWorld* World, const FMOIStateData& StateData)
{
	// Check to make sure NextID represents the next highest ID we can allocate to a new object.
	if (StateData.ID >= NextID)
	{
		NextID = StateData.ID + 1;
	}

	// See if we can restore an identical object that was previously deleted (i.e. by undo, or by clearing a preview delta that created an object)
	AModumateObjectInstance* deletedObj = TryGetDeletedObject(StateData.ID);
	if (deletedObj)
	{
		if ((deletedObj->GetStateData() == StateData) && RestoreObjectImpl(deletedObj))
		{
			return deletedObj;
		}

		// If there was a deleted object with the same ID as our new object, but cannot be restored for the given state data,
		// then fully delete it since its ID must no longer be safely referenced.
		DeletedObjects.FindAndRemoveChecked(StateData.ID)->Destroy();
	}

	if (!ensureAlwaysMsgf(!ObjectsByID.Contains(StateData.ID),
		TEXT("Tried to create a new object with the same ID (%d) as an existing one!"), StateData.ID))
	{
		return nullptr;
	}

	UClass* moiClass = FMOIFactory::GetMOIClass(StateData.ObjectType);
	FActorSpawnParameters moiSpawnParams;
	moiSpawnParams.Owner = World->GetGameState<AEditModelGameState>();
	moiSpawnParams.bNoFail = true;
	AModumateObjectInstance* newObj = World->SpawnActor<AModumateObjectInstance>(moiClass, moiSpawnParams);
	if (!ensure(newObj))
	{
		return newObj;
	}

	ObjectInstanceArray.AddUnique(newObj);
	ObjectsByID.Add(StateData.ID, newObj);

	newObj->SetStateData(StateData);
	newObj->PostCreateObject(true);

	if (bTrackingDeltaObjects)
	{
		DeltaDestroyedObjects.FindOrAdd(newObj->GetObjectType()).Remove(newObj->ID);
		DeltaCreatedObjects.FindOrAdd(newObj->GetObjectType()).Add(newObj->ID);
	}

	return newObj;
}

bool UModumateDocument::ApplyMOIDelta(const FMOIDelta& Delta, UWorld* World)
{
	for (auto& deltaState : Delta.States)
	{
		auto& targetState = deltaState.NewState;

		switch (deltaState.DeltaType)
		{
		case EMOIDeltaType::Create:
		{
			AModumateObjectInstance* newInstance = CreateOrRestoreObj(World, targetState);
			if (ensureAlways(newInstance) && (NextID <= newInstance->ID))
			{
				NextID = newInstance->ID + 1;
			}
		}
		break;

		case EMOIDeltaType::Destroy:
		{
			DeleteObjectImpl(GetObjectById(targetState.ID));
		}
		break;

		case EMOIDeltaType::Mutate:
		{
			AModumateObjectInstance* MOI = GetObjectById(targetState.ID);
			if (ensureAlways(MOI))
			{
				MOI->SetStateData(targetState);
			}
			else
			{
				return false;
			}
		}
		break;

		default:
			ensureAlways(false);
			return false;
		};
	}

	return true;
}

void UModumateDocument::ApplyGraph2DDelta(const FGraph2DDelta &Delta, UWorld *World)
{
	TSharedPtr<FGraph2D> targetSurfaceGraph;

	switch (Delta.DeltaType)
	{
	case EGraph2DDeltaType::Add:
		if (!ensure(!SurfaceGraphs.Contains(Delta.ID)))
		{
			return;
		}
		targetSurfaceGraph = SurfaceGraphs.Add(Delta.ID, MakeShared<FGraph2D>(Delta.ID));
		break;
	case EGraph2DDeltaType::Edit:
	case EGraph2DDeltaType::Remove:
		targetSurfaceGraph = SurfaceGraphs.FindRef(Delta.ID);
		break;
	default:
		break;
	}

	if (!ensure(targetSurfaceGraph.IsValid()))
	{
		return;
	}

	int32 surfaceGraphID = targetSurfaceGraph->GetID();
	AModumateObjectInstance *surfaceGraphObj = GetObjectById(surfaceGraphID);
	if (!ensure(surfaceGraphObj))
	{
		return;
	}

	// update graph itself
	if (!targetSurfaceGraph->ApplyDelta(Delta))
	{
		return;
	}

	// mark the surface graph dirty, so that its children can update their visual and world-coordinate-space representations
	surfaceGraphObj->MarkDirty(EObjectDirtyFlags::Structure);

	// add objects
	for (auto &kvp : Delta.VertexAdditions)
	{
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTSurfaceVertex, surfaceGraphID));
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTSurfaceEdge, surfaceGraphID));
	}

	for (auto &kvp : Delta.PolygonAdditions)
	{
		// It would be ideal to only create SurfacePolgyon objects for interior polygons, but if we don't then the graph will try creating
		// deltas that use IDs that the document will try to re-purpose for other objects.
		// TODO: allow allocating IDs from graph deltas in a way that the document can't use them
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTSurfacePolygon, surfaceGraphID));
	}

	// finalize objects after all of them have been added
	TMap<int32, FGraph2DHostedObjectDelta> ParentIDUpdates;
	FinalizeGraph2DDelta(Delta, ParentIDUpdates);

	// when modifying objects in the document, first add objects, then modify parent objects, then delete objects
	for (auto &kvp : ParentIDUpdates)
	{
		auto obj = GetObjectById(kvp.Key);
		if (obj == nullptr)
		{
			const FGraph2DHostedObjectDelta& objUpdate = kvp.Value;
			auto newParentObj = GetObjectById(objUpdate.NextParentID);
			// When the previous object exists, use its information to clone a new object
			auto hostedObj = GetObjectById(objUpdate.PreviousHostedObjID);
			if (hostedObj)
			{
				auto newStateData = hostedObj->GetStateData();
				newStateData.ID = kvp.Key;
				newStateData.ParentID = objUpdate.NextParentID;
				auto newObj = CreateOrRestoreObj(World, newStateData);
			}
			// Otherwise, attempt to restore the previous object (during undo)
			else
			{
				auto *oldObj = TryGetDeletedObject(kvp.Key);
				if (ensureAlways(oldObj != nullptr))
				{
					RestoreObjectImpl(oldObj);
				}
			}
		}
	}

	// there is a separate loop afterwards because re-parenting objects may rely on objects that are being added
	TSet<int32> objsMarkedForDelete;
	for (auto &kvp : ParentIDUpdates)
	{
		auto obj = GetObjectById(kvp.Key);
		if (obj != nullptr)
		{
			FGraph2DHostedObjectDelta objUpdate = kvp.Value;
			auto newParentObj = GetObjectById(objUpdate.NextParentID);

			if (newParentObj != nullptr)
			{
				FTransform worldTransform = obj->GetWorldTransform();
				obj->SetParentID(newParentObj->ID);
				//obj->SetWorldTransform(worldTransform);
			}
			else
			{
				objsMarkedForDelete.Add(kvp.Key);
			}
		}
	}

	// delete hosted objects
	for (int32 objID : objsMarkedForDelete)
	{
		DeleteObjectImpl(GetObjectById(objID));
	}

	// delete surface objects
	for (auto &kvp : Delta.VertexDeletions)
	{
		DeleteObjectImpl(GetObjectById(kvp.Key));
	}

	for (auto &kvp : Delta.EdgeDeletions)
	{
		DeleteObjectImpl(GetObjectById(kvp.Key));
	}

	for (auto &kvp : Delta.PolygonDeletions)
	{
		DeleteObjectImpl(GetObjectById(kvp.Key));
	}

	// Use the surface graph modified flags, to decide which surface graph object MOIs need to be updated
	TArray<int32> modifiedVertices, modifiedEdges, modifiedPolygons;
	if (targetSurfaceGraph->ClearModifiedObjects(modifiedVertices, modifiedEdges, modifiedPolygons))
	{
		for (int32 vertexID : modifiedVertices)
		{
			AModumateObjectInstance* vertexObj = GetObjectById(vertexID);
			if (ensureAlways(vertexObj && (vertexObj->GetObjectType() == EObjectType::OTSurfaceVertex)))
			{
				vertexObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}

		for (int32 edgeID : modifiedEdges)
		{
			AModumateObjectInstance *edgeObj = GetObjectById(edgeID);
			if (ensureAlways(edgeObj && (edgeObj->GetObjectType() == EObjectType::OTSurfaceEdge)))
			{
				edgeObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}

		for (int32 polygonID : modifiedPolygons)
		{
			AModumateObjectInstance *polygonObj = GetObjectById(polygonID);
			if (ensureAlways(polygonObj && (polygonObj->GetObjectType() == EObjectType::OTSurfacePolygon)))
			{
				polygonObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}
	}

	if (Delta.DeltaType == EGraph2DDeltaType::Remove)
	{
		ensureAlways(targetSurfaceGraph->IsEmpty());
		SurfaceGraphs.Remove(Delta.ID);
	}
}

void UModumateDocument::ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World)
{
	TArray<FVector> controlPoints;

	// TODO: the graph may need an understanding of "net" deleted objects
	// objects that are deleted, but their metadata (like hosted obj) is not
	// passed on to another object
	// if it is passed on to another object, the MarkDirty here is redundant

	// Dirty every group whose members were deleted, or that had members added/removed
	TSet<int32> dirtyGroupIDs;
	for (auto& kvp : Delta.VertexDeletions)
	{
		auto vertex = VolumeGraph.FindVertex(kvp.Key);
		if (vertex == nullptr)
		{
			continue;
		}
		dirtyGroupIDs.Append(vertex->GroupIDs);
	}
	for (auto& kvp : Delta.EdgeDeletions)
	{
		auto edge = VolumeGraph.FindEdge(kvp.Key);
		if (edge == nullptr)
		{
			continue;
		}
		dirtyGroupIDs.Append(edge->GroupIDs);
	}
	for (auto& kvp : Delta.FaceDeletions)
	{
		auto face = VolumeGraph.FindFace(kvp.Key);
		if (face == nullptr)
		{
			continue;
		}
		dirtyGroupIDs.Append(face->GroupIDs);
	}
	for (auto& kvp : Delta.GroupIDsUpdates)
	{
		dirtyGroupIDs.Append(kvp.Value.GroupIDsToAdd);
		dirtyGroupIDs.Append(kvp.Value.GroupIDsToRemove);
	}

	// Dirty every group that was added to or removed from an element, or had members that were deleted without a GroupIDsUpdates delta
	for (int32 groupID : dirtyGroupIDs)
	{
		auto groupObj = GetObjectById(groupID);
		if (groupObj != nullptr)
		{
			groupObj->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	// Dirty every group member that was added or removed from a group
	TArray<int32> updatedGroupMemberIDs;
	Delta.GroupIDsUpdates.GetKeys(updatedGroupMemberIDs);

	for (int32 groupMemberID : updatedGroupMemberIDs)
	{
		if (auto groupMemberObj = GetObjectById(groupMemberID))
		{
			groupMemberObj->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	VolumeGraph.ApplyDelta(Delta);

	for (auto &kvp : Delta.VertexAdditions)
	{
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaVertex));
	}

	for (auto &kvp : Delta.VertexDeletions)
	{
		AModumateObjectInstance *deletedVertexObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedVertexObj))
		{
			DeleteObjectImpl(deletedVertexObj);
		}
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaEdge));
	}

	for (auto &kvp : Delta.EdgeDeletions)
	{
		AModumateObjectInstance *deletedEdgeObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedEdgeObj))
		{
			DeleteObjectImpl(deletedEdgeObj);
		}
	}

	for (auto &kvp : Delta.FaceAdditions)
	{
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaPlane));
	}

	for (auto &kvp : Delta.FaceDeletions)
	{
		AModumateObjectInstance *deletedFaceObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedFaceObj))
		{
			DeleteObjectImpl(deletedFaceObj);
		}
	}
}

bool UModumateDocument::ApplyPresetDelta(const FBIMPresetDelta& PresetDelta, UWorld* World)
{
	/*
	* Invalid GUID->Valid GUID == Make new preset
	* Valid GUID->Valid GUID == Update existing preset
	* Valid GUID->Invalid GUID == Delete existing preset
	*/

	AEditModelGameMode* gameMode = World->GetAuthGameMode<AEditModelGameMode>();

	// Add or update if we have a new GUID
	if (PresetDelta.NewState.GUID.IsValid())
	{
		BIMPresetCollection.AddPreset(PresetDelta.NewState);

		// Find all affected presets and update affected assemblies
		TArray<FGuid> affectedPresets;
		BIMPresetCollection.GetAllAncestorPresets(PresetDelta.NewState.GUID, affectedPresets);
		if (PresetDelta.NewState.ObjectType != EObjectType::OTNone)
		{
			affectedPresets.AddUnique(PresetDelta.NewState.GUID);
		}

		TArray<FGuid> affectedAssemblies;
		for (auto& affectedPreset : affectedPresets)
		{
			// Skip presets that don't define assemblies
			FBIMPresetInstance* preset = BIMPresetCollection.PresetsByGUID.Find(affectedPreset);
			if (!ensureAlways(preset != nullptr) || preset->ObjectType == EObjectType::OTNone)
			{
				continue;
			}
			
			FBIMAssemblySpec newSpec;
			if (ensureAlways(newSpec.FromPreset(*gameMode->ObjectDatabase, BIMPresetCollection, affectedPreset) == EBIMResult::Success))
			{
				affectedAssemblies.Add(affectedPreset);
				BIMPresetCollection.UpdateProjectAssembly(newSpec);
			}
		}

		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
		if (controller && controller->DynamicIconGenerator)
		{
			controller->DynamicIconGenerator->UpdateCachedAssemblies(affectedAssemblies);
		}

		for (auto& moi : ObjectInstanceArray)
		{
			if (affectedAssemblies.Contains(moi->GetAssembly().PresetGUID))
			{
				moi->OnAssemblyChanged();
			}
		}

		return true;
	}
	// Remove if we're deleting an old one (new state has no guid)
	else if (PresetDelta.OldState.GUID.IsValid())
	{
		// TODO: removing presets not yet supported, add scope impact here (fallback assemblies, etc)
		BIMPresetCollection.RemovePreset(PresetDelta.OldState.GUID);
		if (PresetDelta.OldState.ObjectType != EObjectType::OTNone)
		{
			BIMPresetCollection.RemoveProjectAssemblyForPreset(PresetDelta.OldState.GUID);
		}
		return true;
	}

	// Who sent us a delta with no guids?
	return ensure(false);
}

bool UModumateDocument::ApplyDeltas(const TArray<FDeltaPtr>& Deltas, UWorld* World)
{
	if (Deltas.Num() == 0)
	{
		return true;
	}

	StartTrackingDeltaObjects();

	bIsDirty = true;
	ClearPreviewDeltas(World, false);

	ClearRedoBuffer();

	BeginUndoRedoMacro();

	TSharedPtr<UndoRedo> ur = MakeShared<UndoRedo>();
	ur->Deltas = Deltas;

	UndoBuffer.Add(ur);

	// First, apply the input deltas, generated from the first pass of user intent
	for (auto& delta : ur->Deltas)
	{
		delta->ApplyTo(this, World);
	}

	CalculateSideEffectDeltas(ur->Deltas, World);

	// TODO: this should be a proper side effect
	UpdateRoomAnalysis(World);

	EndUndoRedoMacro();

	EndTrackingDeltaObjects();

	return true;
}

bool UModumateDocument::StartPreviewing()
{
	if (bApplyingPreviewDeltas || bFastClearingPreviewDeltas)
	{
		return false;
	}

	bApplyingPreviewDeltas = true;
	PrePreviewNextID = NextID;
	return true;
}

bool UModumateDocument::ApplyPreviewDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World)
{
	ClearPreviewDeltas(World, true);

	if (!bApplyingPreviewDeltas)
	{
		StartPreviewing();
	}

	PreviewDeltas = Deltas;

	// First, apply the input deltas, generated from the first pass of user intent
	for (auto& delta : PreviewDeltas)
	{
		delta->ApplyTo(this, World);
	}

	CalculateSideEffectDeltas(PreviewDeltas, World);

	return true;
}

bool UModumateDocument::IsPreviewingDeltas() const
{
	return bApplyingPreviewDeltas;
}

void UModumateDocument::ClearPreviewDeltas(UWorld *World, bool bFastClear)
{
	if (!bApplyingPreviewDeltas || !ensure(!bFastClearingPreviewDeltas && !bSlowClearingPreviewDeltas))
	{
		return;
	}

	if (bFastClear)
	{
		bFastClearingPreviewDeltas = true;
	}
	else
	{
		bSlowClearingPreviewDeltas = true;
	}

	if (PreviewDeltas.Num() > 0)
	{
		TArray<FDeltaPtr> inversePreviewDeltas = PreviewDeltas;
		Algo::Reverse(inversePreviewDeltas);

		// First, apply the input deltas, generated from the first pass of user intent
		for (auto& delta : inversePreviewDeltas)
		{
			delta->MakeInverse()->ApplyTo(this, World);
		}

		PostApplyDeltas(World);

		PreviewDeltas.Reset();
	}

	if (bSlowClearingPreviewDeltas)
	{
		// Fully delete any objects that were created by preview deltas, in this case found and defined by
		// their ID being greater than the non-preview NextID.
		TArray<int32> invalidDeletedIDs;
		for (auto kvp : DeletedObjects)
		{
			if (kvp.Key >= NextID)
			{
				invalidDeletedIDs.Add(kvp.Key);
			}
		}

		for (int32 invalidDeletedID : invalidDeletedIDs)
		{
			DeletedObjects.FindAndRemoveChecked(invalidDeletedID)->Destroy();
		}
	}

	NextID = PrePreviewNextID;
	bApplyingPreviewDeltas = false;
	bFastClearingPreviewDeltas = false;
	bSlowClearingPreviewDeltas = false;

	CleanObjects(nullptr);
}

void UModumateDocument::CalculateSideEffectDeltas(TArray<FDeltaPtr>& Deltas, UWorld* World)
{
	// Next, clean objects while gathering potential side effect deltas,
	// apply side effect deltas, and add them to the undo/redo-able list of deltas.
	// Prevent infinite loops, but allow for iteration due to multiple levels of dependency.
	int32 sideEffectIterationGuard = 8;
	TArray<FDeltaPtr> sideEffectDeltas;
	do
	{
		sideEffectDeltas.Reset();
		CleanObjects(&sideEffectDeltas);
		for (auto& delta : sideEffectDeltas)
		{
			Deltas.Add(delta);
			delta->ApplyTo(this, World);
		}
		PostApplyDeltas(World);

		if (!ensure(--sideEffectIterationGuard > 0))
		{
			UE_LOG(LogTemp, Error, TEXT("Iterative CleanObjects generated too many side effects; preventing infinite loop!"));
			break;
		}

	} while (sideEffectDeltas.Num() > 0);
}

void UModumateDocument::UpdateVolumeGraphObjects(UWorld *World)
{
	// TODO: unclear whether this is correct or the best place -
	// set the faces containing or contained by dirty faces dirty as well	
	for (auto& kvp : VolumeGraph.GetFaces())
	{
		auto& face = kvp.Value;
		if (!face.bDirty)
		{
			continue;
		}

		if (auto containingFace = VolumeGraph.FindFace(face.ContainingFaceID))
		{
			containingFace->Dirty(false);
		}
		for (int32 containedFaceID : face.ContainedFaceIDs)
		{
			if (auto containedFace = VolumeGraph.FindFace(containedFaceID))
			{
				containedFace->Dirty(false);
			}
		}
	}

	TSet<int32> dirtyGroupIDs;
	TArray<int32> cleanedVertices, cleanedEdges, cleanedFaces;
	if (VolumeGraph.CleanGraph(cleanedVertices, cleanedEdges, cleanedFaces))
	{
		for (int32 vertexID : cleanedVertices)
		{
			FGraph3DVertex *graphVertex = VolumeGraph.FindVertex(vertexID);
			AModumateObjectInstance *vertexObj = GetObjectById(vertexID);
			if (graphVertex && vertexObj && (vertexObj->GetObjectType() == EObjectType::OTMetaVertex))
			{
				vertexObj->MarkDirty(EObjectDirtyFlags::Structure);
				dirtyGroupIDs.Append(graphVertex->GroupIDs);
			}
		}

		for (int32 edgeID : cleanedEdges)
		{
			FGraph3DEdge *graphEdge = VolumeGraph.FindEdge(edgeID);
			AModumateObjectInstance *edgeObj = GetObjectById(edgeID);
			if (graphEdge && edgeObj && (edgeObj->GetObjectType() == EObjectType::OTMetaEdge))
			{
				edgeObj->MarkDirty(EObjectDirtyFlags::Structure);
				dirtyGroupIDs.Append(graphEdge->GroupIDs);
			}
		}

		for (int32 faceID : cleanedFaces)
		{
			FGraph3DFace *graphFace = VolumeGraph.FindFace(faceID);
			AModumateObjectInstance *faceObj = GetObjectById(faceID);
			if (graphFace && faceObj && (faceObj->GetObjectType() == EObjectType::OTMetaPlane))
			{
				faceObj->MarkDirty(EObjectDirtyFlags::Structure);
				dirtyGroupIDs.Append(graphFace->GroupIDs);
			}
		}
	}

	// dirty group objects that are related to dirtied graph objects
	for (int32 groupID : dirtyGroupIDs)
	{
		AModumateObjectInstance *groupObj = GetObjectById(groupID);
		if (groupObj != nullptr)
		{
			groupObj->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}
}

bool UModumateDocument::FinalizeGraphDeltas(const TArray<FGraph3DDelta> &InDeltas, TArray<FDeltaPtr> &OutDeltas)
{
	FGraph3D moiTempGraph;
	FGraph3D::CloneFromGraph(moiTempGraph, VolumeGraph);

	for (auto& delta : InDeltas)
	{
		TArray<FDeltaPtr> sideEffectDeltas;
		if (!FinalizeGraphDelta(moiTempGraph, delta, sideEffectDeltas))
		{
			return false;
		}

		OutDeltas.Add(MakeShared<FGraph3DDelta>(delta));
		OutDeltas.Append(sideEffectDeltas);
	}

	return true;
}

bool UModumateDocument::PostApplyDeltas(UWorld *World)
{
	UpdateVolumeGraphObjects(World);
	FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);

	for (auto& kvp : SurfaceGraphs)
	{
		kvp.Value->CleanDirtyObjects(true);
	}

	// Now that objects may have been deleted, validate the player state so that none of them are incorrectly referenced.
	AEditModelPlayerState *playerState = Cast<AEditModelPlayerState>(World->GetFirstPlayerController()->PlayerState);
	playerState->ValidateSelectionsAndView();

	return true;
}

void UModumateDocument::StartTrackingDeltaObjects()
{
	if (!ensure(!bTrackingDeltaObjects))
	{
		return;
	}

	for (auto& kvp : DeltaCreatedObjects)
	{
		kvp.Value.Reset();
	}

	for (auto& kvp : DeltaDestroyedObjects)
	{
		kvp.Value.Reset();
	}

	bTrackingDeltaObjects = true;
}

void UModumateDocument::EndTrackingDeltaObjects()
{
	if (!ensure(bTrackingDeltaObjects))
	{
		return;
	}

	for (auto& kvp : DeltaCreatedObjects)
	{
		EObjectType objectType = kvp.Key;
		int32 numCreatedObjects = kvp.Value.Num();
		if (numCreatedObjects > 0)
		{
			UModumateAnalyticsStatics::RecordObjectCreation(this, objectType, numCreatedObjects);
			OnAppliedMOIDeltas.Broadcast(objectType, numCreatedObjects, EMOIDeltaType::Create);
		}

		kvp.Value.Reset();
	}

	for (auto& kvp : DeltaDestroyedObjects)
	{
		EObjectType objectType = kvp.Key;
		int32 numDeletedObjects = kvp.Value.Num();
		if (numDeletedObjects > 0)
		{
			UModumateAnalyticsStatics::RecordObjectDeletion(this, objectType, numDeletedObjects);
			OnAppliedMOIDeltas.Broadcast(objectType, numDeletedObjects, EMOIDeltaType::Destroy);
		}

		kvp.Value.Reset();
	}

	bTrackingDeltaObjects = false;
}

void UModumateDocument::DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis, bool bDeleteConnected)
{
	TArray<AModumateObjectInstance*> mois;
	Algo::Transform(obIds,mois,[this](int32 id) { return GetObjectById(id);});

	DeleteObjects(
		mois.FilterByPredicate([](AModumateObjectInstance *ob) { return ob != nullptr; }),
		bAllowRoomAnalysis, bDeleteConnected
	);
}

void UModumateDocument::DeleteObjects(const TArray<AModumateObjectInstance*>& initialObjectsToDelete, bool bAllowRoomAnalysis, bool bDeleteConnected)
{
	TArray<FDeltaPtr> deleteDeltas;
	GetDeleteObjectsDeltas(deleteDeltas, initialObjectsToDelete, bAllowRoomAnalysis, bDeleteConnected);

	UWorld *world = initialObjectsToDelete.Num() > 0 ? initialObjectsToDelete[0]->GetWorld() : nullptr;
	if (world == nullptr)
	{
		return;
	}
	ApplyDeltas(deleteDeltas, world);
}

void UModumateDocument::GetDeleteObjectsDeltas(TArray<FDeltaPtr> &OutDeltas, const TArray<AModumateObjectInstance*> &initialObjectsToDelete, bool bAllowRoomAnalysis, bool bDeleteConnected)
{
	if (initialObjectsToDelete.Num() == 0)
	{
		return;
	}

	UWorld *world = initialObjectsToDelete[0]->GetWorld();
	if (!ensureAlways(world != nullptr))
	{
		return;
	}

	// Keep track of all descendants of intended (and connected) objects to delete
	TSet<AModumateObjectInstance*> allObjectsToDelete(initialObjectsToDelete);

	// Gather 3D graph objects, so we can generated deltas that will potentially delete connected objects.
	TSet<int32> graph3DObjIDsToDelete;
	TSet<AModumateObjectInstance*> graph3DDescendents;
	for (AModumateObjectInstance* objToDelete : initialObjectsToDelete)
	{
		int32 objID = objToDelete->ID;
		EGraph3DObjectType graph3DObjType;
		if (IsObjectInVolumeGraph(objToDelete->ID, graph3DObjType) || VolumeGraph.ContainsGroup(objID))
		{
			graph3DObjIDsToDelete.Add(objID);
		}

		allObjectsToDelete.Append(objToDelete->GetAllDescendents());
	}

	// Generate the delta to apply to the 3D volume graph for object deletion
	TArray<FDeltaPtr> graph3DDeltas;
	if (graph3DObjIDsToDelete.Num() > 0)
	{
		FGraph3DDelta graph3DDelta;
		if (TempVolumeGraph.GetDeltaForDeleteObjects(graph3DObjIDsToDelete.Array(), graph3DDelta, bDeleteConnected) && ensure(!graph3DDelta.IsEmpty()))
		{
			graph3DDelta.AggregateDeletedObjects(graph3DObjIDsToDelete);

			for (int32 graph3DObjID : graph3DObjIDsToDelete)
			{
				AModumateObjectInstance* graph3DObj = GetObjectById(graph3DObjID);
				if (ensure(graph3DObj))
				{
					auto descendants = graph3DObj->GetAllDescendents();
					allObjectsToDelete.Add(graph3DObj);
					allObjectsToDelete.Append(descendants);
					graph3DDescendents.Append(descendants);
				}
			}

			graph3DDeltas.Add(MakeShared<FGraph3DDelta>(graph3DDelta));
		}
	}

	// Group surface graph elements by surface graph, so we can generate deltas for each surface graph separately
	// NOTE: for consistency and less code (at the cost of performance), deleting an entire surface graph (say, as a result of deleting its parent wall)
	// is accomplished by getting the deltas for deleting all of its elements. This also allows undo to add them back as Graph2D element addition deltas.
	TMap<int32, TArray<int32>> surfaceGraphDeletionMap;
	for (AModumateObjectInstance* objToDelete : allObjectsToDelete)
	{
		EObjectType objType = objToDelete->GetObjectType();
		bool bInSurfaceGraph = (UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objType) != EGraphObjectType::None);
		if (bInSurfaceGraph)
		{
			int32 surfaceGraphID = objToDelete->GetParentID();
			if (SurfaceGraphs.Contains(surfaceGraphID))
			{
				surfaceGraphDeletionMap.FindOrAdd(surfaceGraphID).Add(objToDelete->ID);
			}
		}
	}

	// Generate the deltas to apply to the 2D surface graphs for object deletion
	TArray<FGraph2DDelta> tempSurfaceGraphDeltas;
	TArray<FDeltaPtr> combinedSurfaceGraphDeltas;
	TSet<int32> combinedSurfaceGraphObjIDsToDelete;
	TSet<AModumateObjectInstance*> surfaceGraphDescendents;
	for (auto& kvp : surfaceGraphDeletionMap)
	{
		int32 surfaceGraphID = kvp.Key;
		const TArray<int32>& surfaceGraphObjToDelete = kvp.Value;
		auto surfaceGraph = FindSurfaceGraph(surfaceGraphID);
		tempSurfaceGraphDeltas.Reset();
		bool bDeletionSuccess = surfaceGraph->DeleteObjects(tempSurfaceGraphDeltas, NextID, surfaceGraphObjToDelete);
		if (bDeletionSuccess)
		{
			for (FGraph2DDelta& deletionDelta : tempSurfaceGraphDeltas)
			{
				if (ensure((deletionDelta.DeltaType == EGraph2DDeltaType::Remove) || !deletionDelta.IsEmpty()))
				{
					combinedSurfaceGraphDeltas.Add(MakeShared<FGraph2DDelta>(deletionDelta));
					deletionDelta.AggregateDeletedObjects(combinedSurfaceGraphObjIDsToDelete);
				}
			}
		}
	}
	for (int32 surfaceGraphObjID : combinedSurfaceGraphObjIDsToDelete)
	{
		AModumateObjectInstance* surfaceGraphObj = GetObjectById(surfaceGraphObjID);
		if (ensure(surfaceGraphObj))
		{
			auto descendants = surfaceGraphObj->GetAllDescendents();
			allObjectsToDelete.Add(surfaceGraphObj);
			allObjectsToDelete.Append(descendants);
			surfaceGraphDescendents.Append(descendants);
		}
	}

	// Separate the full list of objects to delete, including connections and descendants, so we can delete non-graph objects correctly.
	TArray<const AModumateObjectInstance*> graph3DDerivedObjects, surfaceGraphDerivedObjects, nonGraphDerivedObjects;
	for (AModumateObjectInstance* objToDelete : allObjectsToDelete)
	{
		if (ensure(objToDelete))
		{
			int32 objID = objToDelete->ID;
			EObjectType objType = objToDelete->GetObjectType();
			bool bInGraph3D = (UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objType) != EGraph3DObjectType::None);
			bool bInSurfaceGraph = (UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objType) != EGraphObjectType::None);

			if (bInGraph3D)
			{
				ensure(graph3DObjIDsToDelete.Contains(objID));
			}
			else if (bInSurfaceGraph)
			{
				ensure(combinedSurfaceGraphObjIDsToDelete.Contains(objID));
			}
			else
			{
				if (surfaceGraphDescendents.Contains(objToDelete))
				{
					surfaceGraphDerivedObjects.Add(objToDelete);
				}
				else if (graph3DDescendents.Contains(objToDelete))
				{
					graph3DDerivedObjects.Add(objToDelete);
				}
				else
				{
					nonGraphDerivedObjects.Add(objToDelete);
				}
			}
		}
	}

	auto gatherNonGraphDeletionDeltas = [&OutDeltas](const TArray<const AModumateObjectInstance*>& objects)
	{
		auto deleteDelta = MakeShared<FMOIDelta>();
		for (const AModumateObjectInstance* nonGraphObject : objects)
		{
			deleteDelta->AddCreateDestroyState(nonGraphObject->GetStateData(), EMOIDeltaType::Destroy);
		}
		OutDeltas.Add(deleteDelta);
	};

	// Gather all of the deltas that apply to different types of objects
	gatherNonGraphDeletionDeltas(surfaceGraphDerivedObjects);
	OutDeltas.Append(combinedSurfaceGraphDeltas);
	gatherNonGraphDeletionDeltas(graph3DDerivedObjects);
	OutDeltas.Append(graph3DDeltas);
	gatherNonGraphDeletionDeltas(nonGraphDerivedObjects);

	// TODO: represent room analysis as side effects
}

AModumateObjectInstance *UModumateDocument::TryGetDeletedObject(int32 id)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::TryGetDeletedObject"));
	return DeletedObjects.FindRef(id);
}

int32 UModumateDocument::MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID)
{
	ClearRedoBuffer();

	int id = NextID++;

	TArray<AModumateObjectInstance*> obs;
	Algo::Transform(ids,obs,[this](int32 id){return GetObjectById(id);}); 

	TMap<AModumateObjectInstance*, AModumateObjectInstance*> oldParents;
	for (auto ob : obs)
	{
		oldParents.Add(ob, ob->GetParentObject());
	}

	auto *groupObj = CreateOrRestoreObj(world, FMOIStateData(id, EObjectType::OTGroup, parentID));

	for (auto ob : obs)
	{
		ob->SetParentID(id);
	}

	return id;
}

void UModumateDocument::UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds)
{
	ClearRedoBuffer();

	AEditModelGameMode *gameMode = world->GetAuthGameMode<AEditModelGameMode>();

	TArray<AModumateObjectInstance*> obs;
	Algo::Transform(groupIds,obs,[this](int32 id){return GetObjectById(id);});

	TMap<AModumateObjectInstance*, TArray<AModumateObjectInstance*>> oldChildren;

	for (auto ob : obs)
	{
		TArray<AModumateObjectInstance*> children = ob->GetChildObjects();
		oldChildren.Add(ob, children);
		for (auto child : children)
		{
			child->SetParentID(MOD_ID_NONE);
		}
		DeleteObjectImpl(ob);
	}
}

bool UModumateDocument::GetVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<FGraph3DDelta> deltas;

	if (!TempVolumeGraph.GetDeltaForVertexMovements(VertexIDs, VertexPositions, deltas, NextID))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	if (!FinalizeGraphDeltas(deltas, OutDeltas))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	return true;
}

bool UModumateDocument::GetPreviewVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<FGraph3DDelta> deltas;

	if (!TempVolumeGraph.MoveVerticesDirect(VertexIDs, VertexPositions, deltas, NextID))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	if (!FinalizeGraphDeltas(deltas, OutDeltas))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	return true;
}

bool UModumateDocument::MoveMetaVertices(UWorld* World, const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions)
{
	TArray<FDeltaPtr> deltas;
	if (GetVertexMovementDeltas(VertexIDs, VertexPositions, deltas))
	{
		return ApplyDeltas(deltas, World);
	}

	return false;
}

bool UModumateDocument::JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs)
{
	if (ObjectIDs.Num() < 2)
	{
		return false;
	}

	EGraph3DObjectType objectType;
	if (!IsObjectInVolumeGraph(ObjectIDs[0], objectType))
	{
		return false;
	}

	TArray<FGraph3DDelta> graphDeltas;
	if (!TempVolumeGraph.GetDeltasForObjectJoin(graphDeltas, ObjectIDs, NextID, objectType))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<FDeltaPtr> deltaPtrs;
	if (!FinalizeGraphDeltas(graphDeltas, deltaPtrs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}
	return ApplyDeltas(deltaPtrs, World);
}

int32 UModumateDocument::MakeRoom(UWorld *World, const TArray<FGraphSignedID> &FaceIDs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeRoom"));

	// TODO: reimplement
	//UModumateRoomStatics::SetRoomConfigFromKey(newRoomObj, UModumateRoomStatics::DefaultRoomConfigKey);

	return MOD_ID_NONE;
}

bool UModumateDocument::MakeMetaObject(UWorld* world, const TArray<FVector>& points,
	TArray<int32>& OutObjectIDs, TArray<FDeltaPtr>& OutDeltaPtrs, bool bSplitAndUpdateFaces)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeMetaObject"));
	OutObjectIDs.Reset();
	OutDeltaPtrs.Reset();

	bool bValidDelta = false;
	int32 numPoints = points.Num();

	TArray<FGraph3DDelta> deltas;
	int32 id = MOD_ID_NONE;

	if (numPoints == 1)
	{
		FGraph3DDelta graphDelta;
		bValidDelta = (numPoints == 1) && TempVolumeGraph.GetDeltaForVertexAddition(points[0], graphDelta, NextID, id);
		OutObjectIDs = { id };
		deltas = { graphDelta };
	}
	else if (numPoints == 2)
	{
		bValidDelta = TempVolumeGraph.GetDeltaForEdgeAdditionWithSplit(points[0], points[1], deltas, NextID, OutObjectIDs, true, bSplitAndUpdateFaces);
	}
	else if (numPoints >= 3)
	{
		bValidDelta = TempVolumeGraph.GetDeltaForFaceAddition(points, deltas, NextID, OutObjectIDs, TSet<int32>(), bSplitAndUpdateFaces);
	}
	else
	{
		return false;
	}

	if (!bValidDelta || !FinalizeGraphDeltas(deltas, OutDeltaPtrs))
	{
		// delta will be false if the object exists, out object ids should contain the existing id
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	return bValidDelta;
}

bool UModumateDocument::PasteMetaObjects(const FGraph3DRecord* InRecord, TArray<FDeltaPtr>& OutDeltaPtrs, TMap<int32, TArray<int32>>& OutCopiedToPastedIDs, const FVector &Offset, bool bIsPreview)
{
	// TODO: potentially make this function a bool
	TArray<FGraph3DDelta> OutDeltas;
	TempVolumeGraph.GetDeltasForPaste(InRecord, Offset, NextID, OutDeltas, OutCopiedToPastedIDs, bIsPreview);

	if (!FinalizeGraphDeltas(OutDeltas, OutDeltaPtrs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	return true;
}

bool UModumateDocument::MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height)
{
	// TODO: reimplement
	return false;
}

bool UModumateDocument::FinalizeGraph2DDelta(const FGraph2DDelta &Delta, TMap<int32, FGraph2DHostedObjectDelta> &OutParentIDUpdates)
{
	TMap<int32, TArray<int32>> parentIDToChildrenIDs;

	for (auto &kvp : Delta.PolygonAdditions)
	{
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			if (parentIDToChildrenIDs.Contains(parentID))
			{
				parentIDToChildrenIDs[parentID].Add(kvp.Key);
			}
			else
			{
				parentIDToChildrenIDs.Add(parentID, { kvp.Key });
			}
		}
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			parentID = FMath::Abs(parentID);
			if (parentIDToChildrenIDs.Contains(parentID))
			{
				parentIDToChildrenIDs[parentID].Add(FMath::Abs(kvp.Key));
			}
			else
			{
				parentIDToChildrenIDs.Add(parentID, { FMath::Abs(kvp.Key) });
			}
		}
	}

	TMap<int32, int32> childFaceIDToHostedID;
	TSet<int32> idsWithObjects;
	for (auto &kvp : parentIDToChildrenIDs)
	{
		int32 parentID = kvp.Key;
		auto parentObj = GetObjectById(parentID);

		if (parentObj == nullptr || parentObj->GetChildIDs().Num() == 0)
		{
			continue;
		}

		for (int32 childIdx = 0; childIdx < parentObj->GetChildIDs().Num(); childIdx++)
		{
			auto childObj = GetObjectById(parentObj->GetChildIDs()[childIdx]);

			for (int32 childFaceID : kvp.Value)
			{
				if (!idsWithObjects.Contains(childFaceID))
				{
					int32 newObjID = NextID++;
					OutParentIDUpdates.Add(newObjID, FGraph2DHostedObjectDelta(parentObj->GetChildIDs()[childIdx], MOD_ID_NONE, childFaceID));
					childFaceIDToHostedID.Add(childFaceID, newObjID);
					idsWithObjects.Add(childFaceID);
				}
			}
		}
	}

	TArray<int32> deletedObjIDs;
	for (auto &kvp : Delta.PolygonDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
	}
	for (auto &kvp : Delta.EdgeDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
	}

	for (int32 objID : deletedObjIDs)
	{
		auto obj = GetObjectById(objID);

		if (obj && obj->GetChildIDs().Num() > 0)
		{
			for (int32 childIdx = 0; childIdx < obj->GetChildIDs().Num(); childIdx++)
			{
				OutParentIDUpdates.Add(obj->GetChildIDs()[childIdx], FGraph2DHostedObjectDelta(MOD_ID_NONE, objID, MOD_ID_NONE));
			}
		}
	}
	return true;
}

bool UModumateDocument::FinalizeGraphDelta(Modumate::FGraph3D &TempGraph, const FGraph3DDelta &Delta, TArray<FDeltaPtr> &OutSideEffectDeltas)
{
	TMap<int32, TArray<int32>> parentIDToChildrenIDs;

	for (auto &kvp : Delta.FaceAdditions)
	{
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			if (parentIDToChildrenIDs.Contains(parentID))
			{
				parentIDToChildrenIDs[parentID].Add(kvp.Key);
			}
			else
			{
				parentIDToChildrenIDs.Add(parentID, { kvp.Key });
			}
		}
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			parentID = FMath::Abs(parentID);
			if (parentIDToChildrenIDs.Contains(parentID))
			{
				parentIDToChildrenIDs[parentID].Add(FMath::Abs(kvp.Key));
			}
			else
			{
				parentIDToChildrenIDs.Add(parentID, { FMath::Abs(kvp.Key) });
			}
		}
	}

	// both the temp graph and the original graph are used here -
	// it is necessary to compare the new faces to the old faces in order to figure out where the 
	// child object are supposed to go (the original graph is used to populate parentFace below)
	TempGraph.ApplyDelta(Delta);

	TSet<int32> idsWithObjects;

	for (auto &kvp : parentIDToChildrenIDs)
	{
		int32 parentID = kvp.Key;
		auto parentObj = GetObjectById(parentID);

		// obtained from the original graph because this face is removed by now
		auto parentFace = VolumeGraph.FindFace(parentID);

		if (parentObj && parentObj->GetChildIDs().Num() > 0)
		{
			for (int32 childIdx = 0; childIdx < parentObj->GetChildIDs().Num(); childIdx++)
			{
				auto childObj = GetObjectById(parentObj->GetChildIDs()[childIdx]);
				// wall/floor objects need to be cloned to each child object, and will be deleted during face deletions
				for (int32 childFaceID : kvp.Value)
				{
					if (!idsWithObjects.Contains(childFaceID))
					{
						FMOIStateData stateData = childObj->GetStateData();
						stateData.ID = NextID++;
						stateData.ParentID = childFaceID;

						auto delta = MakeShared<FMOIDelta>();
						delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
						OutSideEffectDeltas.Add(delta);

						idsWithObjects.Add(childFaceID);
					}
				}
			}
		}
	}

	TArray<int32> deletedObjIDs;
	for (auto &kvp : Delta.FaceDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
	}
	for (auto &kvp : Delta.EdgeDeletions)
	{
		deletedObjIDs.Add(kvp.Key);
	}

	for (int32 objID : deletedObjIDs)
	{
		auto obj = GetObjectById(objID);

		if (obj && obj->GetChildIDs().Num() > 0)
		{
			auto& childIDs = obj->GetChildIDs();
			for (int32 childIdx = 0; childIdx < childIDs.Num(); childIdx++)
			{
				auto childObj = GetObjectById(childIDs[childIdx]);
				if (!childObj)
				{
					continue;
				}

				auto deleteObjectDelta = MakeShared<FMOIDelta>();
				deleteObjectDelta->AddCreateDestroyState(childObj->GetStateData(), EMOIDeltaType::Destroy);
				OutSideEffectDeltas.Add(deleteObjectDelta);

				if (childObj->GetChildIDs().Num() == 0)
				{
					continue;
				}

				auto& grandchildIDs = childObj->GetChildIDs();
				for (int32 grandchildIdx = 0; grandchildIdx < grandchildIDs.Num(); grandchildIdx++)
				{
					auto surfaceGraphObj = GetObjectById(grandchildIDs[grandchildIdx]);
					if (!surfaceGraphObj || !SurfaceGraphs.Contains(surfaceGraphObj->ID))
					{
						continue;
					}

					// delete all objects in the surface graph
					auto surfaceGraph = SurfaceGraphs[surfaceGraphObj->ID];
					TArray<FGraph2DDelta> deltas;
					TArray<int32> allIDs;
					surfaceGraph->GetAllObjects().GenerateKeyArray(allIDs);
					surfaceGraph->DeleteObjects(deltas, NextID, allIDs);

					for (auto& delta : deltas)
					{
						OutSideEffectDeltas.Add(MakeShared<FGraph2DDelta>(delta));
					};

					// delete remaining mois (mois reflecting the surface graph are deleted by the FGraph2DDeltas)
					// TODO: consolidate this kind of behavior with DeleteObjects
					TArray<AModumateObjectInstance*> objectsToDelete = surfaceGraphObj->GetAllDescendents();
					objectsToDelete.Add(surfaceGraphObj);

					auto deleteSurfaceDelta = MakeShared<FMOIDelta>();
					for (AModumateObjectInstance* descendent : objectsToDelete)
					{
						if (UModumateTypeStatics::Graph2DObjectTypeFromObjectType(descendent->GetObjectType()) == EGraphObjectType::None)
						{
							deleteSurfaceDelta->AddCreateDestroyState(descendent->GetStateData(), EMOIDeltaType::Destroy);
						}
					}

					OutSideEffectDeltas.Add(deleteSurfaceDelta);
				}
			}
		}
	}

	return true;
}

void UModumateDocument::ClearRedoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearRedoBuffer"));
	RedoBuffer.Empty();
}

void UModumateDocument::ClearUndoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearUndoBuffer"));
	UndoBuffer.Empty();
	UndoRedoMacroStack.Empty();
}

FBoxSphereBounds UModumateDocument::CalculateProjectBounds() const
{
	TArray<FVector> allMOIPoints;
	TArray<FStructurePoint> curMOIPoints;
	TArray<FStructureLine> curMOILines;

	for (const AModumateObjectInstance *moi : ObjectInstanceArray)
	{
		// TODO: add this functionality to MOIs - IsMetaObject(), meta-objects do not contribute 
		// to project bounds
		if (moi->GetObjectType() == EObjectType::OTCutPlane || moi->GetObjectType() == EObjectType::OTScopeBox)
		{
			continue;
		}

		curMOIPoints.Reset();
		curMOILines.Reset();
		moi->GetStructuralPointsAndLines(curMOIPoints, curMOILines);

		for (const FStructurePoint &point : curMOIPoints)
		{
			allMOIPoints.Add(point.Point);
		}
	}

	FBoxSphereBounds projectBounds(allMOIPoints.GetData(), allMOIPoints.Num());
	return projectBounds;
}

int32 UModumateDocument::GetNextAvailableID() const 
{ 
	return NextID; 
}

int32 UModumateDocument::ReserveNextIDs(int32 reservingObjID)
{ 
	if (!ensureAlways(ReservingObjectID == MOD_ID_NONE))
	{
		return MOD_ID_NONE;
	}

	ReservingObjectID = reservingObjID;

	return NextID;
}

void UModumateDocument::SetNextID(int32 ID, int32 reservingObjID)
{
	if (!ensureAlways(ReservingObjectID == reservingObjID) ||
		!ensureAlways(ID >= NextID))
	{
		return;
	}

	NextID = ID;
	ReservingObjectID = MOD_ID_NONE;
}

bool UModumateDocument::CleanObjects(TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	static TMap<int32, EObjectDirtyFlags> curCleanedFlags;
	curCleanedFlags.Reset();

	static TArray<AModumateObjectInstance*> curDirtyList;
	curDirtyList.Reset();

	int32 totalObjectCleans = 0;
	int32 totalObjectsDirty = 0;

	// Prevent iterating over all combined dirty flags too many times while trying to clean all objects
	int32 combinedDirtySafeguard = 4;
	do
	{
		for (EObjectDirtyFlags flagToClean : UModumateTypeStatics::OrderedDirtyFlags)
		{
			// Arbitrarily cut off object cleaning iteration, in case there's bad logic that
			// creates circular dependencies that will never resolve in a single frame.
			bool bModifiedAnyObjects = false;
			int32 objectCleans = 0;
			TArray<AModumateObjectInstance*>& dirtyObjList = DirtyObjectMap.FindOrAdd(flagToClean);

			// Prevent iterating over objects dirtied with this flag too many times while trying to clean all objects
			int32 sameFlagSafeguard = 8;
			do
			{
				// Save off the current list of dirty objects, since it may change while cleaning them.
				bModifiedAnyObjects = false;
				curDirtyList = dirtyObjList;

				for (AModumateObjectInstance *objToClean : curDirtyList)
				{
					EObjectDirtyFlags& cleanedFlags = curCleanedFlags.FindOrAdd(objToClean->ID, EObjectDirtyFlags::None);
					if (!((cleanedFlags & flagToClean) == EObjectDirtyFlags::None))
					{
						UE_LOG(LogTemp, Error, TEXT("Already cleaned %s ID #%d flag %s this frame!"),
							*GetEnumValueString(objToClean->GetObjectType()), objToClean->ID,
							*GetEnumValueString(flagToClean));
					}

					bool bCleaned = objToClean->RouteCleanObject(flagToClean, OutSideEffectDeltas);
					if (bCleaned)
					{
						cleanedFlags |= flagToClean;
					}

					bModifiedAnyObjects |= bCleaned;
					if (bModifiedAnyObjects)
					{
						++objectCleans;
					}
				}

			} while (bModifiedAnyObjects &&
				ensureMsgf(--sameFlagSafeguard > 0, TEXT("Infinite loop detected while cleaning objects with flag %s, breaking!"), *GetEnumValueString(flagToClean)));

			totalObjectCleans += objectCleans;
		}

		totalObjectsDirty = 0;
		for (auto& kvp : DirtyObjectMap)
		{
			totalObjectsDirty += kvp.Value.Num();
		}

	} while ((totalObjectsDirty > 0) &&
		ensureMsgf(--combinedDirtySafeguard > 0, TEXT("Infinite loop detected while cleaning combined dirty flags, breaking!")));

	return (totalObjectCleans > 0);
}

void UModumateDocument::RegisterDirtyObject(EObjectDirtyFlags DirtyType, AModumateObjectInstance *DirtyObj, bool bDirty)
{
	// Make sure only one dirty flag is used at a time
	int32 flagInt = (int32)DirtyType;
	if (!ensureAlwaysMsgf((DirtyObj != nullptr) && (DirtyType != EObjectDirtyFlags::None) && ((flagInt & (flagInt - 1)) == 0),
		TEXT("Must mark a non-null object with exactly one dirty flag type at a time!")))
	{
		return;
	}

	TArray<AModumateObjectInstance*> &dirtyObjList = DirtyObjectMap.FindOrAdd(DirtyType);

	if (bDirty)
	{
		// We can use TArray with Add rather than AddUnique or TSet because objects already check against their own
		// dirty flag before registering themselves with the Document.
		dirtyObjList.Add(DirtyObj);
	}
	else
	{
		dirtyObjList.Remove(DirtyObj);
	}
}

void UModumateDocument::MakeNew(UWorld *World)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeNew"));

	int32 numObjects = ObjectInstanceArray.Num();
	for (int32 i = numObjects - 1; i >= 0; --i)
	{
		AModumateObjectInstance* obj = ObjectInstanceArray[i];

		ObjectsByID.Remove(obj->ID);
		ObjectInstanceArray.RemoveAt(i, 1, false);

		obj->Destroy();
	}

	for (auto& kvp : DeletedObjects)
	{
		if (kvp.Value)
		{
			kvp.Value->Destroy();
		}
	}
	DeletedObjects.Reset();

	for (auto &kvp : DirtyObjectMap)
	{
		kvp.Value.Reset();
	}

	AEditModelGameMode* gameMode = Cast<AEditModelGameMode>(World->GetAuthGameMode());
	BIMPresetCollection = gameMode->ObjectDatabase->GetPresetCollection();
	
	// Clear drafting render directories
	UModumateGameInstance *modGameInst = World ? World->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;
	if (draftMan != nullptr)
	{
		draftMan->Reset();
	}

	NextID = 1;

	ClearRedoBuffer();
	ClearUndoBuffer();

	UndoRedoMacroStack.Reset();
	VolumeGraph.Reset();
	TempVolumeGraph.Reset();
	SurfaceGraphs.Reset();

	SetCurrentProjectPath();

	bIsDirty = false;
}

const AModumateObjectInstance *UModumateDocument::ObjectFromActor(const AActor *actor) const
{
	return const_cast<UModumateDocument*>(this)->ObjectFromActor(const_cast<AActor*>(actor));
}

AModumateObjectInstance *UModumateDocument::ObjectFromSingleActor(AActor *actor)
{
	auto *moiComponent = actor ? actor->FindComponentByClass<UModumateObjectComponent>() : nullptr;
	if (moiComponent && (moiComponent->ObjectID != MOD_ID_NONE))
	{
		return GetObjectById(moiComponent->ObjectID);
	}

	return nullptr;
}

AModumateObjectInstance *UModumateDocument::ObjectFromActor(AActor *actor)
{
	AModumateObjectInstance *moi = ObjectFromSingleActor(actor);

	while ((moi == nullptr) && (actor != nullptr))
	{
		actor = actor->GetAttachParentActor();
		moi = ObjectFromSingleActor(actor);
	}

	return moi;
}

TArray<AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(EObjectType type)
{
	return ObjectInstanceArray.FilterByPredicate([type](AModumateObjectInstance *moi)
	{
		return moi->GetObjectType() == type;
	});
}

TArray<const AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(const FObjectTypeSet& types) const
{
	TArray<const AModumateObjectInstance*> outArray;
	Algo::TransformIf(ObjectInstanceArray, outArray, [&types](AModumateObjectInstance * moi)
		{ return types.Contains(moi->GetObjectType()); },
		[](AModumateObjectInstance * moi) {return moi; });
	return outArray;
}

TArray<AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(const FObjectTypeSet& types)
{
	return ObjectInstanceArray.FilterByPredicate([&types](AModumateObjectInstance *moi)
		{ return types.Contains(moi->GetObjectType()); });
}

void UModumateDocument::GetObjectIdsByAssembly(const FGuid& AssemblyKey, TArray<int32>& OutIds) const
{
	for (const auto &moi : ObjectInstanceArray)
	{
		if (moi->GetAssembly().UniqueKey() == AssemblyKey)
		{
			OutIds.Add(moi->ID);
		}
	}
}

TArray<const AModumateObjectInstance*> UModumateDocument::GetObjectsOfType(EObjectType type) const
{
//	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::GetObjectsOfType"));

	TArray< const AModumateObjectInstance *> outObjectsOfType;
	Algo::Transform(
			ObjectInstanceArray.FilterByPredicate([type](AModumateObjectInstance *moi)
			{
				return moi->GetObjectType() == type;
			}), 
		outObjectsOfType,
			[](AModumateObjectInstance *moi)
			{
				return moi;
			});
	return outObjectsOfType;
}

bool UModumateDocument::ExportDWG(UWorld * world, const TCHAR * filepath)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ExportDWG"));
	CurrentDraftingView = MakeShared<FModumateDraftingView>(world, this, FModumateDraftingView::kDWG);
	CurrentDraftingView->CurrentFilePath = FString(filepath);
	CurrentDraftingView->GeneratePagesFromCutPlanes(world);

	return true;
}

bool UModumateDocument::SerializeRecords(UWorld* World, FModumateDocumentHeader& OutHeader, FMOIDocumentRecord& OutDocumentRecord)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Serialize"));

	AEditModelGameMode *gameMode = Cast<AEditModelGameMode>(World->GetAuthGameMode());

	// Header is its own object
	OutHeader.Version = Modumate::DocVersion;
	OutHeader.Thumbnail = CurrentEncodedThumbnail;

	EToolMode modes[] = {
		EToolMode::VE_PLACEOBJECT,
		EToolMode::VE_WALL,
		EToolMode::VE_FLOOR,
		EToolMode::VE_DOOR,
		EToolMode::VE_WINDOW,
		EToolMode::VE_STAIR,
		EToolMode::VE_RAIL,
		EToolMode::VE_CABINET,
		EToolMode::VE_FINISH,
		EToolMode::VE_COUNTERTOP,
		EToolMode::VE_STRUCTURELINE,
		EToolMode::VE_TRIM,
		EToolMode::VE_ROOF_FACE,
		EToolMode::VE_CEILING
	};

	AEditModelPlayerController* emPlayerController = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
	AEditModelPlayerState* emPlayerState = emPlayerController->EMPlayerState;

	for (auto &mode : modes)
	{
		TScriptInterface<IEditModelToolInterface> tool = emPlayerController->ModeToTool.FindRef(mode);
		if (ensureAlways(tool))
		{
			OutDocumentRecord.CurrentToolAssemblyGUIDMap.Add(mode, tool->GetAssemblyGUID());
		}
	}

	// DDL 2.0
	BIMPresetCollection.SavePresetsToDocRecord(OutDocumentRecord);

	// Capture object instances into doc struct
	for (AModumateObjectInstance* obj : ObjectInstanceArray)
	{
		// Don't save graph-reflected MOIs, since their information is stored in separate graph structures
		EObjectType objectType = obj ? obj->GetObjectType() : EObjectType::OTNone;
		EGraph3DObjectType graph3DType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
		EGraphObjectType graph2DType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objectType);
		if (obj && (graph3DType == EGraph3DObjectType::None) && (graph2DType == EGraphObjectType::None))
		{
			FMOIStateData& stateData = obj->GetStateData();
			stateData.CustomData.SaveJsonFromCbor();
			OutDocumentRecord.ObjectData.Add(stateData);
		}
	}


	VolumeGraph.Save(&OutDocumentRecord.VolumeGraph);

	// Save all of the surface graphs as records
	for (const auto &kvp : SurfaceGraphs)
	{
		auto& surfaceGraph = kvp.Value;
		FGraph2DRecord& surfaceGraphRecord = OutDocumentRecord.SurfaceGraphs.Add(kvp.Key);
		surfaceGraph->ToDataRecord(&surfaceGraphRecord, true, true);
	}

	OutDocumentRecord.CameraViews = SavedCameraViews;

	for (auto& ur : UndoBuffer)
	{
		OutDocumentRecord.AppliedDeltas.Add(FDeltasRecord(ur->Deltas));
	}

	return true;
}

bool UModumateDocument::Save(UWorld* World, const FString& FilePath, bool bSetAsCurrentProject)
{
	FModumateDocumentHeader docHeader;
	FMOIDocumentRecord docRecord;
	if (!SerializeRecords(World, docHeader, docRecord))
	{
		return false;
	}

	TSharedPtr<FJsonObject> FileJson = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> HeaderJson = MakeShared<FJsonObject>();
	FileJson->SetObjectField(DocHeaderField, FJsonObjectConverter::UStructToJsonObject<FModumateDocumentHeader>(docHeader));

	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FMOIDocumentRecord>(docRecord);
	FileJson->SetObjectField(DocObjectInstanceField, docOb);

	FString ProjectJsonString;
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&ProjectJsonString);

	bool bWriteJsonSuccess = FJsonSerializer::Serialize(FileJson.ToSharedRef(), JsonStringWriter);
	if (!bWriteJsonSuccess)
	{
		return false;
	}

	bool bFileSaveSuccess = FFileHelper::SaveStringToFile(ProjectJsonString, *FilePath);

	if (bFileSaveSuccess && bSetAsCurrentProject)
	{
		SetCurrentProjectPath(FilePath);
		bIsDirty = false;

		if (auto* gameInstance = World->GetGameInstance<UModumateGameInstance>())
		{
			gameInstance->UserSettings.RecordRecentProject(FilePath, true);
		}
	}

	return bFileSaveSuccess;
}

AModumateObjectInstance *UModumateDocument::GetObjectById(int32 id)
{
	return ObjectsByID.FindRef(id);
}

const AModumateObjectInstance *UModumateDocument::GetObjectById(int32 id) const
{
	return ObjectsByID.FindRef(id);
}

bool UModumateDocument::Load(UWorld *world, const FString &path, bool bSetAsCurrentProject, bool bRecordAsRecentProject)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Load"));

	//Get player state and tells it to empty selected object
	AEditModelPlayerController* EMPlayerController = Cast<AEditModelPlayerController>(world->GetFirstPlayerController());
	AEditModelPlayerState* EMPlayerState = EMPlayerController->EMPlayerState;
	EMPlayerState->OnNewModel();

	MakeNew(world);

	AEditModelGameMode* gameMode = world->GetAuthGameMode<AEditModelGameMode>();

	FModumateDatabase* objectDB = gameMode->ObjectDatabase;

	FModumateDocumentHeader docHeader;
	FMOIDocumentRecord docRec;

	if (FModumateSerializationStatics::TryReadModumateDocumentRecord(path, docHeader, docRec))
	{
		BIMPresetCollection.ReadPresetsFromDocRecord(*objectDB, docRec);

		// Load the connectivity graphs now, which contain associations between object IDs,
		// so that any objects whose geometry setup needs to know about connectivity can find it.
		VolumeGraph.Load(&docRec.VolumeGraph);
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);

		// Load all of the surface graphs now
		for (const auto& kvp : docRec.SurfaceGraphs)
		{
			const FGraph2DRecord& surfaceGraphRecord = kvp.Value;
			TSharedPtr<FGraph2D> surfaceGraph = MakeShared<FGraph2D>(kvp.Key);
			if (surfaceGraph->FromDataRecord(&surfaceGraphRecord))
			{
				SurfaceGraphs.Add(kvp.Key, surfaceGraph);
			}
		}

		SavedCameraViews = docRec.CameraViews;

		// Create the MOIs whose state data was stored
		NextID = 1;
		for (auto& stateData : docRec.ObjectData)
		{
			CreateOrRestoreObj(world, stateData);
		}

		// Create MOIs reflected from the volume graph
		for (const auto& kvp : VolumeGraph.GetAllObjects())
		{
			EObjectType objectType = UModumateTypeStatics::ObjectTypeFromGraph3DType(kvp.Value);
			if (ensure(!ObjectsByID.Contains(kvp.Key)) && (objectType != EObjectType::OTNone))
			{
				CreateOrRestoreObj(world, FMOIStateData(kvp.Key, objectType));
			}
		}

		// Create MOIs reflected from the surface graphs
		for (const auto& surfaceGraphKVP : SurfaceGraphs)
		{
			if (surfaceGraphKVP.Value.IsValid())
			{
				for (const auto& surfaceGraphObjKVP : surfaceGraphKVP.Value->GetAllObjects())
				{
					EObjectType objectType = UModumateTypeStatics::ObjectTypeFromGraph2DType(surfaceGraphObjKVP.Value);
					if (ensure(!ObjectsByID.Contains(surfaceGraphObjKVP.Key)) && (objectType != EObjectType::OTNone))
					{
						CreateOrRestoreObj(world, FMOIStateData(surfaceGraphObjKVP.Key, objectType, surfaceGraphKVP.Key));
					}
				}
			}
		}

		// Now that all objects have been created and parented correctly, we can clean all of them.
		// This should take care of anything that depends on relationships between objects, like mitering.
		CleanObjects(nullptr);

		// Check for objects that have errors, as a result of having been loaded and cleaned.
		// TODO: prompt the user to fix, or delete, these objects
		for (auto* obj : ObjectInstanceArray)
		{
			if (EMPlayerState->DoesObjectHaveAnyError(obj->ID))
			{
				FString objectTypeString = GetEnumValueString(obj->GetObjectType());
				UE_LOG(LogTemp, Warning, TEXT("MOI %d (%s) has an error!"), obj->ID, *objectTypeString);
			}
		}

		for (auto& cta : docRec.CurrentToolAssemblyGUIDMap)
		{
			TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(cta.Key);
			if (ensureAlways(tool))
			{
				const FBIMAssemblySpec *obAsm = GetPresetCollection().GetAssemblyByGUID(cta.Key, cta.Value);
				if (obAsm != nullptr)
				{
					tool->SetAssemblyGUID(obAsm->UniqueKey());
				}
			}
		}

		// Just in case the loaded document's rooms don't match up with our current room analysis logic, do that now.
		UpdateRoomAnalysis(world);

		// Hide all cut planes on load
		TArray<AModumateObjectInstance*> cutPlanes = GetObjectsOfType(EObjectType::OTCutPlane);
		TArray<int32> hideCutPlaneIds;
		for (auto curCutPlane : cutPlanes)
		{
			hideCutPlaneIds.Add(curCutPlane->ID);
		}
		AddHideObjectsById(world, hideCutPlaneIds);

		ClearUndoBuffer();

		// Load undo/redo buffer
		for (auto& deltaRecord : docRec.AppliedDeltas)
		{
			TSharedPtr<UndoRedo> undoRedo = MakeShared<UndoRedo>();

			for (auto& structWrapper : deltaRecord.DeltaStructWrappers)
			{
				auto deltaPtr = structWrapper.CreateStructFromJSON<FDocumentDelta>();
				if (deltaPtr)
				{
					undoRedo->Deltas.Add(MakeShareable(deltaPtr));
				}
			}

			UndoBuffer.Add(undoRedo);
		}

		if (bSetAsCurrentProject)
		{
			SetCurrentProjectPath(path);
		}

		auto* gameInstance = world->GetGameInstance<UModumateGameInstance>();
		if (bRecordAsRecentProject && gameInstance)
		{
			gameInstance->UserSettings.RecordRecentProject(path, true);
		}

		bIsDirty = false;

		return true;
	}
	return false;
}

bool UModumateDocument::LoadDeltas(UWorld* world, const FString& path, bool bSetAsCurrentProject, bool bRecordAsRecentProject)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Load"));

	//Get player state and tells it to empty selected object
	AEditModelPlayerController* EMPlayerController = Cast<AEditModelPlayerController>(world->GetFirstPlayerController());
	AEditModelPlayerState* EMPlayerState = EMPlayerController->EMPlayerState;
	EMPlayerState->OnNewModel();

	MakeNew(world);

	AEditModelGameMode* gameMode = world->GetAuthGameMode<AEditModelGameMode>();

	FModumateDatabase* objectDB = gameMode->ObjectDatabase;

	FModumateDocumentHeader docHeader;
	FMOIDocumentRecord docRec;

	if (FModumateSerializationStatics::TryReadModumateDocumentRecord(path, docHeader, docRec))
	{
		// Load all of the deltas into the redo buffer, as if the user had undone all the way to the beginning
		// the redo buffer expects the deltas to all be backwards
		for (int urIdx = docRec.AppliedDeltas.Num() - 1; urIdx >= 0; urIdx--)
		{
			auto& deltaRecord = docRec.AppliedDeltas[urIdx];
			TSharedPtr<UndoRedo> undoRedo = MakeShared<UndoRedo>();

			for (int deltaIdx = 0; deltaIdx < deltaRecord.DeltaStructWrappers.Num(); deltaIdx++)
			{
				auto& structWrapper = deltaRecord.DeltaStructWrappers[deltaIdx];
				if (!ensure(structWrapper.LoadFromJson()))
				{
					continue;
				}

				auto deltaPtr = structWrapper.CreateStructFromJSON<FDocumentDelta>();
				if (deltaPtr)
				{
					undoRedo->Deltas.Add(MakeShareable(deltaPtr));
				}
			}

			// Copied from PerformUndoRedo, apply deltas to the redo buffer in this way so that 
			// when they are popped off, they are applied in the right direction
			TArray<FDeltaPtr> fromDeltas = undoRedo->Deltas;
			Algo::Reverse(fromDeltas);

			undoRedo->Deltas.Empty();
			Algo::Transform(fromDeltas, undoRedo->Deltas, [](const FDeltaPtr& DeltaPtr) {return DeltaPtr->MakeInverse(); });
			RedoBuffer.Add(undoRedo);
		}
	}

	return true;
}

TArray<int32> UModumateDocument::CloneObjects(UWorld *world, const TArray<int32> &objs, const FTransform& offsetTransform)
{
	// make a list of MOI objects from the IDs, send them to the MOI version of this function return array of IDs.

	TArray<AModumateObjectInstance*> obs;
	Algo::Transform(objs,obs,[this](auto id) {return GetObjectById(id); });

	obs = CloneObjects(
		world,
		obs,
		offsetTransform)
		.FilterByPredicate([](AModumateObjectInstance *ob) {return ob != nullptr; });

	TArray<int32> ids;

	Algo::Transform(obs,ids,[this](auto *ob){return ob->ID;});

	return ids;
}

int32 UModumateDocument::CloneObject(UWorld *world, const AModumateObjectInstance *original)
{
	// TODO: reimplement
	return MOD_ID_NONE;
}

TArray<AModumateObjectInstance *> UModumateDocument::CloneObjects(UWorld *world, const TArray<AModumateObjectInstance *> &objs, const FTransform& offsetTransform)
{
	TMap<AModumateObjectInstance*, AModumateObjectInstance*> oldToNew;

	BeginUndoRedoMacro();

	// Step 1 of clone macro: deserialize the objects, each of which will have its own undo/redo step
	TArray<AModumateObjectInstance*> newObjs;
	Algo::Transform(
		objs,
		newObjs,
		[this,world,&oldToNew,offsetTransform](AModumateObjectInstance *obj)
		{
			AModumateObjectInstance *newOb = GetObjectById(CloneObject(world, obj));
			oldToNew.Add(obj, newOb);
			return newOb;
		}
	);

	// Set up the correct parenting for the newly-cloned objects
	for (auto newObj : newObjs)
	{
		AModumateObjectInstance *oldParent = newObj->GetParentObject();
		AModumateObjectInstance *newParent = oldToNew.FindRef(oldParent);

		if (oldParent && newParent)
		{
			newObj->SetParentID(newParent->ID);
		}
	}

	EndUndoRedoMacro();

	return newObjs;
}

void UModumateDocument::SetCurrentProjectPath(const FString& currentProjectPath)
{
	CurrentProjectPath = currentProjectPath;
	CurrentProjectName = currentProjectPath;

	if (!currentProjectPath.IsEmpty())
	{
		FString projectDir, projectExt;
		FPaths::Split(CurrentProjectPath, projectDir, CurrentProjectName, projectExt);
	}

	UModumateFunctionLibrary::SetWindowTitle(CurrentProjectName);
}

void UModumateDocument::UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs)
{
	TSet<int32> dirtyPlaneIDs;

	// Gather all of the dirty planes that need to be updated with new miter geometry
	for (int32 dirtyObjID : dirtyObjIDs)
	{
		AModumateObjectInstance *dirtyObj = GetObjectById(dirtyObjID);
		if (dirtyObj)
		{
			switch (dirtyObj->GetObjectType())
			{
			case EObjectType::OTMetaEdge:
			{
				const FGraph3DEdge *graphEdge = VolumeGraph.FindEdge(dirtyObjID);
				for (const FEdgeFaceConnection &edgeFaceConnection : graphEdge->ConnectedFaces)
				{
					dirtyPlaneIDs.Add(FMath::Abs(edgeFaceConnection.FaceID));
				}
				break;
			}
			case EObjectType::OTMetaPlane:
			{
				dirtyPlaneIDs.Add(dirtyObjID);

				const FGraph3DFace *graphFace = VolumeGraph.FindFace(dirtyObjID);
				graphFace->GetAdjacentFaceIDs(dirtyPlaneIDs);
				break;
			}
			case EObjectType::OTMetaVertex:
			{
				const FGraph3DVertex *graphVertex = VolumeGraph.FindVertex(dirtyObjID);
				graphVertex->GetConnectedFaces(dirtyPlaneIDs);
				break;
			}
			default:
			{
				AModumateObjectInstance *parent = dirtyObj->GetParentObject();
				if (parent && (parent->GetObjectType() == EObjectType::OTMetaPlane))
				{
					dirtyPlaneIDs.Add(parent->ID);
				}
				break;
			}
			}
		}
	}

	// Miter-aware geometry updates only require UpdateGeometry, which in turn will analyze neighboring objects in the graph.
	for (int32 dirtyPlaneID : dirtyPlaneIDs)
	{
		AModumateObjectInstance *dirtyPlane = GetObjectById(dirtyPlaneID);
		FGraph3DFace *dirtyFace = VolumeGraph.FindFace(dirtyPlaneID);
		if (dirtyPlane && dirtyFace)
		{
			const TArray<int32> &childIDs = dirtyPlane->GetChildIDs();
			if (childIDs.Num() == 1)
			{
				if (AModumateObjectInstance *dirtyPlaneHostedObj = GetObjectById(childIDs[0]))
				{
					dirtyPlaneHostedObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}
		}
	}
}

bool UModumateDocument::CanRoomContainFace(FGraphSignedID FaceID)
{
	const FGraph3DFace *graphFace = VolumeGraph.FindFace(FaceID);
	const AModumateObjectInstance *planeObj = GetObjectById(FMath::Abs(FaceID));

	if ((graphFace == nullptr) || (planeObj == nullptr))
	{
		return false;
	}

	// Only allow traversing to planes facing upwards (even slightly)
	FVector signedFaceNormal = FMath::Sign(FaceID) * FVector(graphFace->CachedPlane);
	float normalUpDot = (signedFaceNormal | FVector::UpVector);
	if (FMath::IsNearlyZero(normalUpDot) || (normalUpDot < 0.0f))
	{
		return false;
	}

	// Only allow traversing to planes that have a Floor child
	const TArray<int32> &planeChildIDs = planeObj->GetChildIDs();
	for (int32 planeChildID : planeChildIDs)
	{
		const AModumateObjectInstance *planeChildObj = GetObjectById(planeChildID);
		if (planeChildObj && (planeChildObj->GetObjectType() == EObjectType::OTFloorSegment))
		{
			return true;
		}
	}

	return false;
}

void UModumateDocument::UpdateRoomAnalysis(UWorld *world)
{
#if 0
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UpdateRoomAnalysis"));

	bool bStartedMacro = false;
	bool bAnyChange = false;
	TMap<int32, int32> oldRoomIDsToNewRoomIndices;
	TMap<int32, TArray<int32>> newRoomsFaceIDs;
	TSet<int32> oldRoomsToDeleteIDs, newRoomsToCreateIndices;

	UModumateRoomStatics::CalculateRoomChanges(this, bAnyChange,
		oldRoomIDsToNewRoomIndices, newRoomsFaceIDs, oldRoomsToDeleteIDs, newRoomsToCreateIndices);

	if (bAnyChange)
	{
		// We actually have to make a change to rooms - start the undo/redo operations
		BeginUndoRedoMacro();
		ClearRedoBuffer();
		bStartedMacro = true;

		// Step 1 - modify old rooms
		UndoRedo *urModifyOldRooms = new UndoRedo();

		urModifyOldRooms->Redo = [this, world, urModifyOldRooms, oldRoomIDsToNewRoomIndices, newRoomsFaceIDs]()
		{
			TMap<int32, TArray<int32>> oldRoomsFaceIDs;

			// Use our room mapping to make the changes from old rooms to new rooms
			for (auto &oldToNew : oldRoomIDsToNewRoomIndices)
			{
				int32 oldRoomObjID = oldToNew.Key;
				int32 newRoomIndex = oldToNew.Value;

				AModumateObjectInstance *oldRoomObj = GetObjectById(oldRoomObjID);
				const TArray<int32> *newRoomFaceIDs = newRoomsFaceIDs.Find(newRoomIndex);
				if (oldRoomObj && newRoomFaceIDs)
				{
					// Keep track of the room's previous face IDs
					oldRoomsFaceIDs.Add(oldRoomObjID, oldRoomObj->GetControlPointIndices());

					oldRoomObj->SetControlPointIndices(*newRoomFaceIDs);
					oldRoomObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}

			urModifyOldRooms->Undo = [this, world, urModifyOldRooms, oldRoomIDsToNewRoomIndices, oldRoomsFaceIDs]()
			{
				// Use our room mapping to restore the old rooms
				for (auto &oldRoomKVP : oldRoomsFaceIDs)
				{
					AModumateObjectInstance *oldRoomObj = GetObjectById(oldRoomKVP.Key);
					if (oldRoomObj)
					{
						oldRoomObj->SetControlPointIndices(oldRoomKVP.Value);
						oldRoomObj->MarkDirty(EObjectDirtyFlags::Structure);
					}
				}
			};
		};

		UndoBuffer.Add(urModifyOldRooms);
		urModifyOldRooms->Redo();

		// Step 2 - create new rooms that don't have a mapping from an old room
		for (int32 newRoomIndex : newRoomsToCreateIndices)
		{
			TArray<int32> *newRoomFaceIDs = newRoomsFaceIDs.Find(newRoomIndex);
			if (newRoomFaceIDs)
			{
				MakeRoom(world, *newRoomFaceIDs);
			}
		}

		// Step 3 - delete old rooms that don't have a mapping to a new room
		TArray<int32> oldRoomsToDeleteIDsArray = oldRoomsToDeleteIDs.Array();
		DeleteObjects(oldRoomsToDeleteIDsArray, false, false);
	}

	// Step 1/4 - re-sequence rooms to have the correct room numbers
	TMap<int32, FString> oldRoomNumbers, newRoomNumbers;
	UModumateRoomStatics::CalculateRoomNumbers(this, oldRoomNumbers, newRoomNumbers);
	if (newRoomNumbers.Num() > 0)
	{
		// Start the macro if we haven't already, since there might not have been any room content changes,
		// but there may have been room number changes. Room numbering changes can occur independently,
		// so these undo/redoable actions are separated.
		if (!bStartedMacro)
		{
			BeginUndoRedoMacro();
			ClearRedoBuffer();
			bStartedMacro = true;
		}

		UndoRedo *urSequenceRooms = new UndoRedo();

		urSequenceRooms->Redo = [this, urSequenceRooms, oldRoomNumbers, newRoomNumbers]()
		{
			for (auto &kvp : newRoomNumbers)
			{
				AModumateObjectInstance *roomObj = GetObjectById(kvp.Key);
				const FString &newRoomNumber = kvp.Value;
				roomObj->SetProperty(EScope::Room, BIMPropertyNames::Number, newRoomNumber);
			}

			urSequenceRooms->Undo = [this, oldRoomNumbers]()
			{
				for (auto &kvp : oldRoomNumbers)
				{
					AModumateObjectInstance *roomObj = GetObjectById(kvp.Key);
					const FString &oldRoomNumber = kvp.Value;
					roomObj->SetProperty(EScope::Room, BIMPropertyNames::Number, oldRoomNumber);
				}
			};
		};

		UndoBuffer.Add(urSequenceRooms);
		urSequenceRooms->Redo();
	}

	if (bStartedMacro)
	{
		EndUndoRedoMacro();
	}
#endif
}

const TSharedPtr<Modumate::FGraph2D> UModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID) const
{
	return SurfaceGraphs.FindRef(SurfaceGraphID);
}

TSharedPtr<Modumate::FGraph2D> UModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID)
{
	return SurfaceGraphs.FindRef(SurfaceGraphID);
}

const TSharedPtr<Modumate::FGraph2D> UModumateDocument::FindSurfaceGraphByObjID(int32 ObjectID) const
{
	return const_cast<UModumateDocument*>(this)->FindSurfaceGraphByObjID(ObjectID);
}

TSharedPtr<Modumate::FGraph2D> UModumateDocument::FindSurfaceGraphByObjID(int32 ObjectID)
{
	auto moi = GetObjectById(ObjectID);
	if (moi == nullptr)
	{
		return nullptr;
	}
	int32 parentID = moi->GetParentID();
	TSharedPtr<Modumate::FGraph2D> surfaceGraph = nullptr;

	// this handles the case where the object is a surface graph object
	surfaceGraph = FindSurfaceGraph(parentID);
	if (surfaceGraph)
	{
		return surfaceGraph;
	}

	// this handles the case where the object is a surface graph-hosted object
	auto parentMoi = GetObjectById(parentID);
	if (parentMoi)
	{
		surfaceGraph = FindSurfaceGraph(parentMoi->GetParentID());
	}

	return surfaceGraph;
}

bool UModumateDocument::IsObjectInVolumeGraph(int32 ObjID, EGraph3DObjectType &OutObjType) const
{
	bool bIsInGraph = false;

	const AModumateObjectInstance *moi = GetObjectById(ObjID);
	if (moi == nullptr)
	{
		return bIsInGraph;
	}

	OutObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType());
	bool bIsVolumeGraphType = (OutObjType != EGraph3DObjectType::None);
	bIsInGraph = VolumeGraph.ContainsObject(ObjID);
	ensureAlways(bIsVolumeGraphType == bIsInGraph);

	return bIsInGraph;
}

void UModumateDocument::DisplayDebugInfo(UWorld* world)
{
	auto displayMsg = [](const FString &msg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, msg, false);
	};


	auto displayObjectCount = [this,displayMsg](EObjectType ot, const TCHAR *name)
	{
		TArray<AModumateObjectInstance*> obs = GetObjectsOfType(ot);
		if (obs.Num() > 0)
		{
			displayMsg(FString::Printf(TEXT("%s - %d"), name, obs.Num()));
		}
	};

	AEditModelPlayerState* emPlayerState = Cast<AEditModelPlayerState>(world->GetFirstPlayerController()->PlayerState);

	displayMsg(TEXT("OBJECT COUNTS"));
	auto objectTypeEnum = StaticEnum<EObjectType>();
	for (int32 objectTypeIdx = 0; objectTypeIdx < objectTypeEnum->NumEnums(); ++objectTypeIdx)
	{
		EObjectType objectType = static_cast<EObjectType>(objectTypeEnum->GetValueByIndex(objectTypeIdx));
		displayObjectCount(objectType, *GetEnumValueString(objectType));
	}

	TSet<FGuid> asms;

	Algo::Transform(ObjectInstanceArray, asms,
		[](const AModumateObjectInstance *ob)
		{
			return ob->GetAssembly().UniqueKey();
		}
	);

	for (auto &a : asms)
	{
		int32 instanceCount = Algo::Accumulate(
			ObjectInstanceArray, 0,
			[a](int32 total, const AModumateObjectInstance *ob)
			{
				return ob->GetAssembly().UniqueKey() == a ? total + 1 : total;
			}
		);

		displayMsg(FString::Printf(TEXT("Assembly %s: %d"), *a.ToString(), instanceCount));
	}

	displayMsg(FString::Printf(TEXT("Undo Buffer: %d"), UndoBuffer.Num()));
	displayMsg(FString::Printf(TEXT("Redo Buffer: %d"), RedoBuffer.Num()));
	displayMsg(FString::Printf(TEXT("Deleted Obs: %d"), DeletedObjects.Num()));
	displayMsg(FString::Printf(TEXT("Active Obs: %d"), ObjectInstanceArray.Num()));
	displayMsg(FString::Printf(TEXT("Next ID: %d"), NextID));

	FString selected = FString::Printf(TEXT("Selected Obs: %d"), emPlayerState->SelectedObjects.Num());
	if (emPlayerState->SelectedObjects.Num() > 0)
	{
		selected += TEXT(" | ");
		for (auto &sel : emPlayerState->SelectedObjects)
		{
			selected += FString::Printf(TEXT("SEL: %d #CHILD: %d PARENTID: %d"), sel->ID, sel->GetChildIDs().Num(),sel->GetParentID());
		}
	}
	displayMsg(selected);
}

void UModumateDocument::DrawDebugVolumeGraph(UWorld* world)
{
	const float drawVerticalOffset = 5.0f;
	const float pointThickness = 8.0f;
	const float lineThickness = 2.0f;
	const float arrowSize = 10.0f;
	const float faceEdgeOffset = 15.0f;
	const FVector textOffset = 20.0f * FVector::UpVector;

	for (const auto &kvp : VolumeGraph.GetVertices())
	{
		const FGraph3DVertex &graphVertex = kvp.Value;
		FVector vertexDrawPos = graphVertex.Position;

		world->LineBatcher->DrawPoint(vertexDrawPos, FLinearColor::Red, pointThickness, 0);
		FString vertexString = FString::Printf(TEXT("Vertex #%d: [%s]%s"), graphVertex.ID,
			*FString::JoinBy(graphVertex.ConnectedEdgeIDs, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
			(graphVertex.GroupIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" {%s}"), *FString::JoinBy(graphVertex.GroupIDs, TEXT(", "), [](const int32 &GroupID) { return FString::Printf(TEXT("%d"), GroupID); }))
		);

		DrawDebugString(world, vertexDrawPos + textOffset, vertexString, nullptr, FColor::White, 0.0f, true);
	}

	for (const auto &kvp : VolumeGraph.GetEdges())
	{
		const FGraph3DEdge &graphEdge = kvp.Value;
		const FGraph3DVertex *startGraphVertex = VolumeGraph.FindVertex(graphEdge.StartVertexID);
		const FGraph3DVertex *endGraphVertex = VolumeGraph.FindVertex(graphEdge.EndVertexID);
		if (startGraphVertex && endGraphVertex)
		{
			FVector startDrawPos = startGraphVertex->Position;
			FVector endDrawPos = endGraphVertex->Position;

			DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Blue, false, -1.f, 0xFF, lineThickness);
			FString edgeString = FString::Printf(TEXT("Edge #%d: [%d, %d]%s"), graphEdge.ID, graphEdge.StartVertexID, graphEdge.EndVertexID,
				(graphEdge.GroupIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" {%s}"), *FString::JoinBy(graphEdge.GroupIDs, TEXT(", "), [](const int32 &GroupID) { return FString::Printf(TEXT("%d"), GroupID); })));
			DrawDebugString(world, 0.5f * (startDrawPos + endDrawPos) + textOffset, edgeString, nullptr, FColor::White, 0.0f, true);
		}
	}

	for (const auto &kvp : VolumeGraph.GetFaces())
	{
		const FGraph3DFace &face = kvp.Value;

		int32 numEdges = face.EdgeIDs.Num();
		for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
		{
			FGraphSignedID edgeID = face.EdgeIDs[edgeIdx];
			const FVector &edgeNormal = face.CachedEdgeNormals[edgeIdx];
			const FVector &prevEdgeNormal = face.CachedEdgeNormals[(edgeIdx + numEdges - 1) % numEdges];
			const FVector &nextEdgeNormal = face.CachedEdgeNormals[(edgeIdx + 1) % numEdges];

			const FGraph3DEdge *graphEdge = VolumeGraph.FindEdge(edgeID);
			const FGraph3DVertex *startGraphVertex = VolumeGraph.FindVertex(graphEdge->StartVertexID);
			const FGraph3DVertex *endGraphVertex = VolumeGraph.FindVertex(graphEdge->EndVertexID);

			bool bEdgeForward = (edgeID > 0);
			FVector startDrawPos = bEdgeForward ? startGraphVertex->Position : endGraphVertex->Position;
			FVector endDrawPos = bEdgeForward ? endGraphVertex->Position : startGraphVertex->Position;
			startDrawPos += faceEdgeOffset * (edgeNormal + prevEdgeNormal).GetSafeNormal();
			endDrawPos += faceEdgeOffset * (edgeNormal + nextEdgeNormal).GetSafeNormal();

			FVector edgeNStartPos = (startDrawPos + endDrawPos) / 2.0f;
			FVector edgeNEndPos = edgeNStartPos + edgeNormal * faceEdgeOffset;

			DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Green, false, -1.f, 0, lineThickness);
			DrawDebugDirectionalArrow(world, edgeNStartPos, edgeNEndPos, arrowSize, FColor::Blue, false, -1.f, 0, lineThickness);
		}

		FString faceString = FString::Printf(TEXT("Face #%d: [%s]%s%s%s"), face.ID,
			*FString::JoinBy(face.EdgeIDs, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
			(face.GroupIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" Groups{%s}"), *FString::JoinBy(face.GroupIDs, TEXT(", "), [](const int32 &GroupID) { return FString::Printf(TEXT("%d"), GroupID); })),
			(face.ContainedFaceIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" Contains{%s}"), *FString::JoinBy(face.ContainedFaceIDs, TEXT(", "), [](const int32& ContainedFaceID) { return FString::Printf(TEXT("%d"), ContainedFaceID); })),
			(face.ContainingFaceID == MOD_ID_NONE) ? TEXT("") : *FString::Printf(TEXT(" ContainedBy #%d"), face.ContainingFaceID)
		);
		DrawDebugString(world, face.CachedCenter, faceString, nullptr, FColor::White, 0.0f, true);

		FVector planeNormal = face.CachedPlane;
		DrawDebugDirectionalArrow(world, face.CachedCenter, face.CachedCenter + 2.0f * faceEdgeOffset * planeNormal, arrowSize, FColor::Green, false, -1.f, 0xFF, lineThickness);
	}

	for (const auto &kvp : VolumeGraph.GetPolyhedra())
	{
		const FGraph3DPolyhedron &polyhedron = kvp.Value;
		FString polyhedronFacesString = FString::JoinBy(polyhedron.FaceIDs, TEXT(", "), [](const FGraphSignedID &faceID) { return FString::Printf(TEXT("%d"), faceID); });
		FString polyhedronTypeString = !polyhedron.bClosed ? FString(TEXT("open")) :
			FString::Printf(TEXT("closed, %s, %s"),
				polyhedron.bInterior ? TEXT("interior") : TEXT("exterior"),
				polyhedron.bConvex ? TEXT("convex") : TEXT("concave")
			);
		FString polyhedronString = FString::Printf(TEXT("Polyhedron: #%d: [%s], %s"),
			polyhedron.ID, *polyhedronFacesString, *polyhedronTypeString
		);
		GEngine->AddOnScreenDebugMessage(polyhedron.ID, 0.0f, FColor::White, polyhedronString, false);
	}
}

void UModumateDocument::DrawDebugSurfaceGraphs(UWorld* world)
{
	const float drawVerticalOffset = 5.0f;
	const float pointThickness = 8.0f;
	const float lineThickness = 2.0f;
	const float arrowSize = 10.0f;
	const float faceEdgeOffset = 15.0f;
	const FVector textOffset = 10.0f * FVector::UpVector;

	for (auto& kvp : SurfaceGraphs)
	{
		auto graph = kvp.Value;

		int32 surfaceGraphID = kvp.Key;
		const AModumateObjectInstance *surfaceGraphObj = GetObjectById(surfaceGraphID);
		const AModumateObjectInstance *surfaceGraphParent = surfaceGraphObj ? surfaceGraphObj->GetParentObject() : nullptr;
		int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceGraphObj);
		if (!ensure(surfaceGraphObj && surfaceGraphParent && (surfaceGraphFaceIndex != INDEX_NONE)))
		{
			continue;
		}

		const AMOISurfaceGraph* surfaceGraphImpl = static_cast<const AMOISurfaceGraph*>(surfaceGraphObj);
		FString surfaceGraphString = FString::Printf(TEXT("SurfaceGraph: #%d, face %d, %s"),
			surfaceGraphID, surfaceGraphFaceIndex, surfaceGraphImpl->IsGraphLinked() ? TEXT("linked") : TEXT("unlinked"));
		GEngine->AddOnScreenDebugMessage(surfaceGraphID, 0.0f, FColor::White, surfaceGraphString);

		TArray<FVector> facePoints;
		FVector faceNormal, faceAxisX, faceAxisY;
		if (!ensure(UModumateObjectStatics::GetGeometryFromFaceIndex(surfaceGraphParent, surfaceGraphFaceIndex, facePoints, faceNormal, faceAxisX, faceAxisY)))
		{
			continue;
		}
		FVector faceOrigin = facePoints[0];

		for (auto& vertexkvp : graph->GetVertices())
		{
			const FGraph2DVertex &graphVertex = vertexkvp.Value;
			FVector2D vertexPos = graphVertex.Position;
			FVector vertexDrawPos = UModumateGeometryStatics::Deproject2DPoint(vertexPos, faceAxisX, faceAxisY, faceOrigin);

			world->LineBatcher->DrawPoint(vertexDrawPos, FLinearColor::Red, pointThickness, 0);
			FString vertexString = FString::Printf(TEXT("Vertex #%d: [%s]"), graphVertex.ID,
				*FString::JoinBy(graphVertex.Edges, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); })
			);

			DrawDebugString(world, vertexDrawPos + textOffset, vertexString, nullptr, FColor::White, 0.0f, true);
		}

		for (const auto &edgekvp : graph->GetEdges())
		{
			const FGraph2DEdge &graphEdge = edgekvp.Value;
			const FGraph2DVertex *startGraphVertex = graph->FindVertex(graphEdge.StartVertexID);
			const FGraph2DVertex *endGraphVertex = graph->FindVertex(graphEdge.EndVertexID);
			if (startGraphVertex && endGraphVertex)
			{
				FVector2D startPos = startGraphVertex->Position;
				FVector2D endPos = endGraphVertex->Position;
				FVector startDrawPos = UModumateGeometryStatics::Deproject2DPoint(startPos, faceAxisX, faceAxisY, faceOrigin);
				FVector endDrawPos = UModumateGeometryStatics::Deproject2DPoint(endPos, faceAxisX, faceAxisY, faceOrigin);

				DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Blue, false, -1.f, 0xFF, lineThickness);
				FString edgeString = FString::Printf(TEXT("Edge #%d: [%d, %d], {%d, %d}"), graphEdge.ID, graphEdge.StartVertexID, graphEdge.EndVertexID, graphEdge.LeftPolyID, graphEdge.RightPolyID);
				DrawDebugString(world, 0.5f * (startDrawPos + endDrawPos) + textOffset, edgeString, nullptr, FColor::White, 0.0f, true);
			}
		}

		for (const auto &polykvp : graph->GetPolygons())
		{
			const FGraph2DPolygon &poly = polykvp.Value;

			int32 numEdges = poly.Edges.Num();

			TArray<FVector2D> edgeNormals;
			for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
			{
				FGraphSignedID edgeID = poly.Edges[edgeIdx];
				const FGraph2DEdge *graphEdge = graph->FindEdge(edgeID);
				const FGraph2DVertex *startGraphVertex = graph->FindVertex(graphEdge->StartVertexID);
				const FGraph2DVertex *endGraphVertex = graph->FindVertex(graphEdge->EndVertexID);

				FVector2D edgeNormal = FVector2D(graphEdge->CachedEdgeDir.Y, -graphEdge->CachedEdgeDir.X);
				edgeNormal *= edgeID < 0 ? -1 : 1;
				edgeNormals.Add(edgeNormal);
			}

			for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
			{
				FGraphSignedID edgeID = poly.Edges[edgeIdx];

				const FVector2D &edgeNormal = edgeNormals[edgeIdx];
				const FVector2D &prevEdgeNormal = edgeNormals[(edgeIdx + numEdges - 1) % numEdges];
				const FVector2D &nextEdgeNormal = edgeNormals[(edgeIdx + 1) % numEdges];

				const FGraph2DEdge *graphEdge = graph->FindEdge(edgeID);
				const FGraph2DVertex *startGraphVertex = graph->FindVertex(graphEdge->StartVertexID);
				const FGraph2DVertex *endGraphVertex = graph->FindVertex(graphEdge->EndVertexID);

				bool bEdgeForward = (edgeID > 0);
				FVector2D startPos = bEdgeForward ? startGraphVertex->Position : endGraphVertex->Position;
				FVector2D endPos = bEdgeForward ? endGraphVertex->Position : startGraphVertex->Position;
				startPos += faceEdgeOffset * (edgeNormal + prevEdgeNormal).GetSafeNormal();
				endPos += faceEdgeOffset * (edgeNormal + nextEdgeNormal).GetSafeNormal();

				FVector startDrawPos = UModumateGeometryStatics::Deproject2DPoint(startPos, faceAxisX, faceAxisY, faceOrigin);
				FVector endDrawPos = UModumateGeometryStatics::Deproject2DPoint(endPos, faceAxisX, faceAxisY, faceOrigin);

				FVector edgeNStartPos = UModumateGeometryStatics::Deproject2DPoint(
					(startPos + endPos) / 2.0f, faceAxisX, faceAxisY, faceOrigin);
				FVector edgeNEndPos = UModumateGeometryStatics::Deproject2DPoint(
					(startPos + endPos) / 2.0f + edgeNormal * faceEdgeOffset, faceAxisX, faceAxisY, faceOrigin);

				DrawDebugDirectionalArrow(world, startDrawPos, endDrawPos, arrowSize, FColor::Green, false, -1.f, 0, lineThickness);
				DrawDebugDirectionalArrow(world, edgeNStartPos, edgeNEndPos, arrowSize, FColor::Blue, false, -1.f, 0, lineThickness);
			}

			FString faceString = FString::Printf(TEXT("Face #%d: [%s] (%d)"), poly.ID,
				*FString::JoinBy(poly.Edges, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
				poly.ContainingPolyID);

			FVector2D originPos = Algo::Accumulate(poly.CachedPoints, FVector2D::ZeroVector) / poly.CachedPoints.Num();
			FVector originDrawPos = UModumateGeometryStatics::Deproject2DPoint(originPos, faceAxisX, faceAxisY, faceOrigin);
			DrawDebugString(world, originDrawPos, faceString, nullptr, FColor::White, 0.0f, true);
		}
	}
}

const FBIMPresetCollection& UModumateDocument::GetPresetCollection() const
{
	return BIMPresetCollection;
}

FBIMPresetCollection& UModumateDocument::GetPresetCollection()
{
	return BIMPresetCollection;
}

bool UModumateDocument::MakeNewGUIDForPreset(FBIMPresetInstance& Preset)
{
	return BIMPresetCollection.GetAvailableGUID(Preset.GUID) == EBIMResult::Success;
}
