// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateDocument.h"

#include "Online/ModumateAnalyticsStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/LineActor.h"

#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

#include "Graph/Graph2D.h"
#include "Graph/Graph3DDelta.h"

#include "ModumateCore/PlatformFunctions.h"
#include "Drafting/DraftingManager.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "DocumentManagement/DocumentDelta.h"
#include "Drafting/ModumateDraftingView.h"
#include "ModumateCore/ModumateMitering.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

#include "Algo/Transform.h"
#include "Algo/Accumulate.h"
#include "Algo/ForEach.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "Database/ModumateObjectDatabase.h"
#include "UnrealClasses/ModumateObjectComponent_CPP.h"

#include "UnrealClasses/Modumate.h"

using namespace Modumate::Mitering;
using namespace Modumate;


const FName FModumateDocument::DocumentHideRequestTag(TEXT("DocumentHide"));

FModumateDocument::FModumateDocument() :
	NextID(1)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ModumateDocument"));

	GatherDocumentMetadata();

	//Set Default Values
	//Values will be deprecated in favor of instance-level overrides for assemblies
	DefaultWallHeight = 243.84f;
	ElevationDelta = 0;
	DefaultWindowHeight = 91.44f;
	DefaultDoorHeight = 0.f;
}

FModumateDocument::~FModumateDocument()
{
}

void FModumateDocument::PerformUndoRedo(UWorld* World, TArray<TSharedPtr<UndoRedo>>& FromBuffer, TArray<TSharedPtr<UndoRedo>>& ToBuffer)
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

		PostApplyDeltas(World);
		UpdateRoomAnalysis(World);

		AEditModelPlayerState_CPP* EMPlayerState = Cast<AEditModelPlayerState_CPP>(World->GetFirstPlayerController()->PlayerState);
		EMPlayerState->RefreshActiveAssembly();

		// TODO: currently needed to correctly update graph dimension strings, but it might be
		// better to separate that part out of PostSelectionOrViewChanged
		EMPlayerState->PostSelectionOrViewChanged();

#if WITH_EDITOR
		ensureAlways(fromBufferSize == FromBuffer.Num());
		ensureAlways(toBufferSize == ToBuffer.Num());
#endif
	}
}

void FModumateDocument::Undo(UWorld *World)
{
	if (ensureAlways(!InUndoRedoMacro()))
	{
		PerformUndoRedo(World, UndoBuffer, RedoBuffer);
	}
}

void FModumateDocument::Redo(UWorld *World)
{
	if (ensureAlways(!InUndoRedoMacro()))
	{
		PerformUndoRedo(World, RedoBuffer, UndoBuffer);
	}
}

void FModumateDocument::BeginUndoRedoMacro()
{
	UndoRedoMacroStack.Push(UndoBuffer.Num());
}

void FModumateDocument::EndUndoRedoMacro()
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

bool FModumateDocument::InUndoRedoMacro() const
{
	return (UndoRedoMacroStack.Num() > 0);
}

void FModumateDocument::SetDefaultWallHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultWallHeight"));
	ClearRedoBuffer();
	float orig = DefaultWallHeight;
	DefaultWallHeight = height;
}

void FModumateDocument::SetDefaultRailHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight"));
	ClearRedoBuffer();
	float orig = DefaultRailHeight;
	DefaultRailHeight = height;
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultRailHeight::Redo"));
}

void FModumateDocument::SetDefaultCabinetHeight(float height)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultCabinetHeight"));
	ClearRedoBuffer();
	float orig = DefaultCabinetHeight;
	DefaultCabinetHeight = height;
}

void FModumateDocument::SetDefaultDoorHeightWidth(float height, float width)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultDoorHeightWidth"));
	ClearRedoBuffer();
	float origHeight = DefaultDoorHeight;
	float origWidth = DefaultDoorWidth;
	DefaultDoorHeight = height;
	DefaultDoorWidth = width;
}

void FModumateDocument::SetDefaultWindowHeightWidth(float height, float width)
{
	float origHeight = DefaultWindowHeight;
	float origWidth = DefaultWindowWidth;
	DefaultWindowHeight = height;
	DefaultWindowWidth = width;
}

void FModumateDocument::SetDefaultJustificationZ(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationZ"));
	ClearRedoBuffer();
	float orig = DefaultJustificationZ;
	DefaultJustificationZ = newValue;
}

void FModumateDocument::SetDefaultJustificationXY(float newValue)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::SetDefaultJustificationXY"));
	ClearRedoBuffer();
	float orig = DefaultJustificationXY;
	DefaultJustificationXY = newValue;
}

void FModumateDocument::SetAssemblyForObjects(UWorld *world,TArray<int32> ids, const FBIMAssemblySpec &assembly)
{
	if (ids.Num() == 0)
	{
		return;
	}

	auto delta = MakeShared<FMOIDelta>();
	for (auto id : ids)
	{
		FModumateObjectInstance* obj = GetObjectById(id);
		if (obj != nullptr)
		{
			auto& newState = delta->AddMutationState(obj);
			newState.AssemblyKey = assembly.UniqueKey();
		}
	}

	ApplyDeltas({ delta }, world);
}

void FModumateDocument::AddHideObjectsById(UWorld *world, const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AddHideObjectsById"));

	for (auto id : ids)
	{
		FModumateObjectInstance *obj = GetObjectById(id);
		if (obj && !HiddenObjectsID.Contains(id))
		{
			obj->RequestHidden(FModumateDocument::DocumentHideRequestTag, true);
			obj->RequestCollisionDisabled(FModumateDocument::DocumentHideRequestTag, true);
			HiddenObjectsID.Add(id);
		}
	}
}

void FModumateDocument::UnhideAllObjects(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnhideAllObjects"));

	TSet<int32> ids = HiddenObjectsID;

	for (auto id : ids)
	{
		if (FModumateObjectInstance *obj = GetObjectById(id))
		{
			obj->RequestHidden(FModumateDocument::DocumentHideRequestTag, false);
			obj->RequestCollisionDisabled(FModumateDocument::DocumentHideRequestTag, false);
		}
	}

	HiddenObjectsID.Empty();
	// TODO: Pending removal
	for (FModumateObjectInstance *obj : ObjectInstanceArray)
	{
		obj->UpdateVisibilityAndCollision();
	}
}

void FModumateDocument::UnhideObjectsById(UWorld *world, const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::UnhideObjectsById"));

	for (auto id : ids)
	{
		FModumateObjectInstance *obj = GetObjectById(id);
		if (obj && HiddenObjectsID.Contains(id))
		{
			obj->RequestHidden(FModumateDocument::DocumentHideRequestTag, false);
			obj->RequestCollisionDisabled(FModumateDocument::DocumentHideRequestTag, false);
			HiddenObjectsID.Remove(id);
		}
	}
	// TODO: Pending removal
	for (FModumateObjectInstance *obj : ObjectInstanceArray)
	{
		obj->UpdateVisibilityAndCollision();
	}
}

void FModumateDocument::RestoreDeletedObjects(const TArray<int32> &ids)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::RestoreDeletedObjects"));
	if (ids.Num() == 0)
	{
		return;
	}

	ClearRedoBuffer();
	TArray<FModumateObjectInstance*> oldObs;
	for (auto id : ids)
	{
		FModumateObjectInstance *oldOb = TryGetDeletedObject(id);
		if (oldOb != nullptr)
		{
			RestoreObjectImpl(oldOb);
			oldObs.AddUnique(oldOb);
		}
	}
}

bool FModumateDocument::DeleteObjectImpl(FModumateObjectInstance *ob, bool keepInDeletedList)
{
	if (ob && !ob->IsDestroyed())
	{
		// Store off the connected objects, in case they will be affected by this deletion
		TArray<FModumateObjectInstance *> connectedMOIs;
		ob->GetConnectedMOIs(connectedMOIs);

		ob->Destroy();
		ObjectInstanceArray.Remove(ob);
		ObjectsByID.Remove(ob->ID);

		if (keepInDeletedList)
		{
			DeletedObjects.Add(ob->ID, ob);
		}
		else
		{
			delete ob;
		}

		// Update visibility & collision enabled on neighbors, in case it was dependent on this MOI.
		for (FModumateObjectInstance *obj : connectedMOIs)
		{
			obj->MarkDirty(EObjectDirtyFlags::Mitering);
			obj->MarkDirty(EObjectDirtyFlags::Visuals);
		}

		return true;
	}

	return false;
}

bool FModumateDocument::RestoreObjectImpl(FModumateObjectInstance *obj)
{
	if (obj && (DeletedObjects.FindRef(obj->ID) == obj))
	{
		DeletedObjects.Remove(obj->ID);
		ObjectInstanceArray.AddUnique(obj);
		ObjectsByID.Add(obj->ID, obj);
		obj->RestoreActor();
		obj->PostCreateObject(false);

		return true;
	}

	return false;
}

FModumateObjectInstance* FModumateDocument::CreateOrRestoreObj(UWorld* World, const FMOIStateData& StateData)
{
	// Check to make sure NextID represents the next highest ID we can allocate to a new object.
	if (StateData.ID >= NextID)
	{
		NextID = StateData.ID + 1;
	}

	FModumateObjectInstance* obj = TryGetDeletedObject(StateData.ID);
	if (obj && RestoreObjectImpl(obj))
	{
		return obj;
	}
	else
	{
		if (!ensureAlwaysMsgf(!ObjectsByID.Contains(StateData.ID),
			TEXT("Tried to create a new object with the same ID (%d) as an existing one!"), StateData.ID))
		{
			return nullptr;
		}

		obj = new FModumateObjectInstance(World, this, StateData);
		ObjectInstanceArray.AddUnique(obj);
		ObjectsByID.Add(StateData.ID, obj);

		obj->SetStateData(StateData);
		obj->PostCreateObject(true);

		UModumateAnalyticsStatics::RecordObjectCreation(World, StateData.ObjectType);

		return obj;
	}

	return nullptr;
}

bool FModumateDocument::ApplyMOIDelta(const FMOIDelta& Delta, UWorld* World)
{
	for (auto& deltaState : Delta.States)
	{
		auto& targetState = deltaState.NewState;

		switch (deltaState.DeltaType)
		{
		case EMOIDeltaType::Create:
		{
			FModumateObjectInstance* newInstance = CreateOrRestoreObj(World, targetState);
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
			FModumateObjectInstance* MOI = GetObjectById(targetState.ID);
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

void FModumateDocument::ApplyGraph2DDelta(const FGraph2DDelta &Delta, UWorld *World)
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
	FModumateObjectInstance *surfaceGraphObj = GetObjectById(surfaceGraphID);
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
				newParentObj->AddChild_DEPRECATED(obj);
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
			FModumateObjectInstance* vertexObj = GetObjectById(vertexID);
			if (ensureAlways(vertexObj && (vertexObj->GetObjectType() == EObjectType::OTSurfaceVertex)))
			{
				vertexObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}

		for (int32 edgeID : modifiedEdges)
		{
			FModumateObjectInstance *edgeObj = GetObjectById(edgeID);
			if (ensureAlways(edgeObj && (edgeObj->GetObjectType() == EObjectType::OTSurfaceEdge)))
			{
				edgeObj->MarkDirty(EObjectDirtyFlags::Structure);
			}
		}

		for (int32 polygonID : modifiedPolygons)
		{
			FModumateObjectInstance *polygonObj = GetObjectById(polygonID);
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

void FModumateDocument::ApplyGraph3DDelta(const FGraph3DDelta &Delta, UWorld *World)
{

	TArray<FVector> controlPoints;

	// TODO: the graph may need an understanding of "net" deleted objects
	// objects that are deleted, but their metadata (like hosted obj) is not
	// passed on to another object
	// if it is passed on to another object, the MarkDirty here is redundant
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

	for (int32 groupID : dirtyGroupIDs)
	{
		auto groupObj = GetObjectById(groupID);
		if (groupObj != nullptr)
		{
			groupObj->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	VolumeGraph.ApplyDelta(Delta);

	for (auto &kvp : Delta.VertexAdditions)
	{
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaVertex));
	}

	for (auto &kvp : Delta.VertexDeletions)
	{
		FModumateObjectInstance *deletedVertexObj = GetObjectById(kvp.Key);
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
		FModumateObjectInstance *deletedEdgeObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedEdgeObj))
		{
			DeleteObjectImpl(deletedEdgeObj);
		}
	}

	for (auto &kvp : Delta.FaceAdditions)
	{
		CreateOrRestoreObj(World, FMOIStateData(kvp.Key, EObjectType::OTMetaPlane));
	}

	// when modifying objects in the document, first add objects, then modify parent objects, then delete objects
	for (auto &kvp : Delta.ParentIDUpdates)
	{
		auto obj = GetObjectById(kvp.Key);
		if (obj == nullptr)
		{
			const FGraph3DHostedObjectDelta& objUpdate = kvp.Value;
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
	for (auto &kvp : Delta.ParentIDUpdates)
	{
		auto obj = GetObjectById(kvp.Key);
		if (obj != nullptr)
		{
			FGraph3DHostedObjectDelta objUpdate = kvp.Value;
			auto newParentObj = GetObjectById(objUpdate.NextParentID);

			if (newParentObj != nullptr)
			{
				// Save off the world transform of the portal, since we want it to be the same regardless
				// of its changed relative transforms between its old and new parents.
				FTransform worldTransform = obj->GetWorldTransform();
				newParentObj->AddChild_DEPRECATED(obj);
				//obj->SetWorldTransform(worldTransform);
			}
			else
			{
				objsMarkedForDelete.Add(kvp.Key);
			}
		}
	}

	for (auto objID : objsMarkedForDelete)
	{
		DeleteObjectImpl(GetObjectById(objID));
	}

	for (auto &kvp : Delta.FaceDeletions)
	{
		FModumateObjectInstance *deletedFaceObj = GetObjectById(kvp.Key);
		if (ensureAlways(deletedFaceObj))
		{
			DeleteObjectImpl(deletedFaceObj);
		}
	}
}

bool FModumateDocument::ApplyDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World)
{
	ClearPreviewDeltas(World);

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
	PostApplyDeltas(World);

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
			ur->Deltas.Add(delta);
			delta->ApplyTo(this, World);
		}
		PostApplyDeltas(World);

		if (!ensure(--sideEffectIterationGuard > 0))
		{
			UE_LOG(LogTemp, Error, TEXT("Iterative CleanObjects generated too many side effects; preventing infinite loop!"));
			break;
		}

	} while (sideEffectDeltas.Num() > 0);

	// TODO: this should be a proper side effect
	UpdateRoomAnalysis(World);

	EndUndoRedoMacro();

	return true;
}

bool FModumateDocument::ApplyPreviewDeltas(const TArray<FDeltaPtr> &Deltas, UWorld *World)
{
	ClearPreviewDeltas(World);

	PreviewDeltas = Deltas;

	// First, apply the input deltas, generated from the first pass of user intent
	for (auto& delta : PreviewDeltas)
	{
		delta->ApplyTo(this, World);
	}

	PostApplyDeltas(World);

	return true;
}

void FModumateDocument::ClearPreviewDeltas(UWorld *World)
{
	if (PreviewDeltas.Num() == 0)
	{
		return;
	}

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

void FModumateDocument::UpdateVolumeGraphObjects(UWorld *World)
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
			FModumateObjectInstance *vertexObj = GetObjectById(vertexID);
			if (graphVertex && vertexObj && (vertexObj->GetObjectType() == EObjectType::OTMetaVertex))
			{
				vertexObj->MarkDirty(EObjectDirtyFlags::Structure);
				dirtyGroupIDs.Append(graphVertex->GroupIDs);
			}
		}

		for (int32 edgeID : cleanedEdges)
		{
			FGraph3DEdge *graphEdge = VolumeGraph.FindEdge(edgeID);
			FModumateObjectInstance *edgeObj = GetObjectById(edgeID);
			if (graphEdge && edgeObj && (edgeObj->GetObjectType() == EObjectType::OTMetaEdge))
			{
				edgeObj->MarkDirty(EObjectDirtyFlags::Structure);
				dirtyGroupIDs.Append(graphEdge->GroupIDs);
			}
		}

		for (int32 faceID : cleanedFaces)
		{
			FGraph3DFace *graphFace = VolumeGraph.FindFace(faceID);
			FModumateObjectInstance *faceObj = GetObjectById(faceID);
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
		FModumateObjectInstance *groupObj = GetObjectById(groupID);
		if (groupObj != nullptr)
		{
			groupObj->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}
}

bool FModumateDocument::FinalizeGraphDeltas(TArray <FGraph3DDelta> &Deltas, TArray<int32> &OutAddedFaceIDs, TArray<int32> &OutAddedVertexIDs, TArray<int32> &OutAddedEdgeIDs)
{
	FGraph3D moiTempGraph;
	FGraph3D::CloneFromGraph(moiTempGraph, VolumeGraph);

	for (auto& delta : Deltas)
	{
		if (!FinalizeGraphDelta(moiTempGraph, delta))
		{
			return false;
		}

		for (auto &kvp : delta.FaceAdditions)
		{
			if (kvp.Value.ParentObjIDs.Num() == 1 && GetObjectById(kvp.Value.ParentObjIDs[0]) == nullptr)
			{
				OutAddedFaceIDs.Add(kvp.Key);
			}
		}
		for (auto &kvp : delta.VertexAdditions)
		{
			OutAddedVertexIDs.Add(kvp.Key);
		}
		for (auto &kvp : delta.EdgeAdditions)
		{
			OutAddedEdgeIDs.Add(kvp.Key);
		}

		for (auto &kvp : delta.FaceDeletions)
		{
			OutAddedFaceIDs.Remove(kvp.Key);
		}
		for (auto &kvp : delta.VertexDeletions)
		{
			OutAddedVertexIDs.Remove(kvp.Key);
		}
		for (auto &kvp : delta.EdgeDeletions)
		{
			OutAddedEdgeIDs.Remove(kvp.Key);
		}
	}

	for (int32 faceID : OutAddedFaceIDs)
	{
		if (moiTempGraph.FindOverlappingFace(faceID) != MOD_ID_NONE)
		{
			return false;
		}
	}

	return true;
}

bool FModumateDocument::PostApplyDeltas(UWorld *World)
{
	UpdateVolumeGraphObjects(World);
	FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);

	for (auto& kvp : SurfaceGraphs)
	{
		kvp.Value->CleanDirtyObjects(true);
	}

	// Now that objects may have been deleted, validate the player state so that none of them are incorrectly referenced.
	AEditModelPlayerState_CPP *playerState = Cast<AEditModelPlayerState_CPP>(World->GetFirstPlayerController()->PlayerState);
	playerState->ValidateSelectionsAndView();

	return true;
}

void FModumateDocument::DeleteObjects(const TArray<int32> &obIds, bool bAllowRoomAnalysis, bool bDeleteConnected)
{
	TArray<FModumateObjectInstance*> mois;
	Algo::Transform(obIds,mois,[this](int32 id) { return GetObjectById(id);});

	DeleteObjects(
		mois.FilterByPredicate([](FModumateObjectInstance *ob) { return ob != nullptr; }),
		bAllowRoomAnalysis, bDeleteConnected
	);
}

void FModumateDocument::DeleteObjects(const TArray<FModumateObjectInstance*> &initialObjectsToDelete, bool bAllowRoomAnalysis, bool bDeleteConnected)
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
	TSet<FModumateObjectInstance*> allObjectsToDelete(initialObjectsToDelete);
	TArray<FDeltaPtr> combinedDeltas;

	// Gather 3D graph objects, so we can generated deltas that will potentially delete connected objects.
	TSet<int32> graph3DObjIDsToDelete;
	TSet<FModumateObjectInstance*> graph3DDescendents;
	for (FModumateObjectInstance* objToDelete : initialObjectsToDelete)
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
				FModumateObjectInstance* graph3DObj = GetObjectById(graph3DObjID);
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
	for (FModumateObjectInstance* objToDelete : allObjectsToDelete)
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
	TSet<FModumateObjectInstance*> surfaceGraphDescendents;
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
		FModumateObjectInstance* surfaceGraphObj = GetObjectById(surfaceGraphObjID);
		if (ensure(surfaceGraphObj))
		{
			auto descendants = surfaceGraphObj->GetAllDescendents();
			allObjectsToDelete.Add(surfaceGraphObj);
			allObjectsToDelete.Append(descendants);
			surfaceGraphDescendents.Append(descendants);
		}
	}

	// Separate the full list of objects to delete, including connections and descendants, so we can delete non-graph objects correctly.
	TArray<const FModumateObjectInstance*> graph3DDerivedObjects, surfaceGraphDerivedObjects, nonGraphDerivedObjects;
	for (FModumateObjectInstance* objToDelete : allObjectsToDelete)
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

			UModumateAnalyticsStatics::RecordObjectDeletion(world, objType);
		}
	}

	auto gatherNonGraphDeletionDeltas = [&combinedDeltas](const TArray<const FModumateObjectInstance*>& objects)
	{
		auto deleteDelta = MakeShared<FMOIDelta>();
		for (const FModumateObjectInstance* nonGraphObject : objects)
		{
			deleteDelta->AddCreateDestroyState(nonGraphObject->GetStateData(), EMOIDeltaType::Destroy);
		}
		combinedDeltas.Add(deleteDelta);
	};

	// Gather all of the deltas that apply to different types of objects
	gatherNonGraphDeletionDeltas(surfaceGraphDerivedObjects);
	combinedDeltas.Append(combinedSurfaceGraphDeltas);
	gatherNonGraphDeletionDeltas(graph3DDerivedObjects);
	combinedDeltas.Append(graph3DDeltas);
	gatherNonGraphDeletionDeltas(nonGraphDerivedObjects);

	// Finally, apply the combined deltas for all types of objects to delete
	ApplyDeltas(combinedDeltas, world);

	// TODO: represent room analysis as side effects
}

FModumateObjectInstance *FModumateDocument::TryGetDeletedObject(int32 id)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::TryGetDeletedObject"));
	return DeletedObjects.FindRef(id);
}

int32 FModumateDocument::MakeGroupObject(UWorld *world, const TArray<int32> &ids, bool combineWithExistingGroups, int32 parentID)
{
	ClearRedoBuffer();

	int id = NextID++;

	TArray<FModumateObjectInstance*> obs;
	Algo::Transform(ids,obs,[this](int32 id){return GetObjectById(id);}); 

	TMap<FModumateObjectInstance*, FModumateObjectInstance*> oldParents;
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

void FModumateDocument::UnmakeGroupObjects(UWorld *world, const TArray<int32> &groupIds)
{
	ClearRedoBuffer();

	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();

	TArray<FModumateObjectInstance*> obs;
	Algo::Transform(groupIds,obs,[this](int32 id){return GetObjectById(id);});

	TMap<FModumateObjectInstance*, TArray<FModumateObjectInstance*>> oldChildren;

	for (auto ob : obs)
	{
		TArray<FModumateObjectInstance*> children = ob->GetChildObjects();
		oldChildren.Add(ob, children);
		for (auto child : children)
		{
			child->SetParentID(MOD_ID_NONE);
		}
		DeleteObjectImpl(ob);
	}
}

bool FModumateDocument::GetVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<FGraph3DDelta> deltas;

	if (!TempVolumeGraph.GetDeltaForVertexMovements(VertexIDs, VertexPositions, deltas, NextID))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<int32> faceIDs, vertexIDs, edgeIDs;
	if (!FinalizeGraphDeltas(deltas, faceIDs, vertexIDs, edgeIDs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	for (auto& delta : deltas)
	{
		OutDeltas.Add(MakeShared<FGraph3DDelta>(delta));
	}

	return true;
}

bool FModumateDocument::GetPreviewVertexMovementDeltas(const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<FGraph3DDelta> deltas;

	if (!TempVolumeGraph.MoveVerticesDirect(VertexIDs, VertexPositions, deltas, NextID))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<int32> faceIDs, vertexIDs, edgeIDs;
	if (!FinalizeGraphDeltas(deltas, faceIDs, vertexIDs, edgeIDs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	for (auto& delta : deltas)
	{
		OutDeltas.Add(MakeShared<FGraph3DDelta>(delta));
	}

	return true;
}

bool FModumateDocument::MoveMetaVertices(UWorld* World, const TArray<int32>& VertexIDs, const TArray<FVector>& VertexPositions)
{
	TArray<FDeltaPtr> deltas;
	if (GetVertexMovementDeltas(VertexIDs, VertexPositions, deltas))
	{
		return ApplyDeltas(deltas, World);
	}

	return false;
}

bool FModumateDocument::JoinMetaObjects(UWorld *World, const TArray<int32> &ObjectIDs)
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

	TArray<int32> faceIDs, vertexIDs, edgeIDs;
	if (!FinalizeGraphDeltas(graphDeltas, faceIDs, vertexIDs, edgeIDs))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<FDeltaPtr> deltaPtrs;
	for (auto& delta : graphDeltas)
	{
		deltaPtrs.Add(MakeShared<FGraph3DDelta>(delta));
	}
	return ApplyDeltas(deltaPtrs, World);
}

int32 FModumateDocument::MakeRoom(UWorld *World, const TArray<FGraphSignedID> &FaceIDs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeRoom"));

	// TODO: reimplement
	//UModumateRoomStatics::SetRoomConfigFromKey(newRoomObj, UModumateRoomStatics::DefaultRoomConfigKey);

	return MOD_ID_NONE;
}

bool FModumateDocument::MakeMetaObject(UWorld *world, const TArray<FVector> &points, const TArray<int32> &IDs, EObjectType objectType, int32 parentID, TArray<int32> &OutObjIDs)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeMetaObject"));
	OutObjIDs.Reset();

	EGraph3DObjectType graphObjectType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
	if (!ensureAlways(graphObjectType != EGraph3DObjectType::None))
	{
		return false;
	}

	bool bValidDelta = false;
	int32 numPoints = points.Num();
	int32 numIDs = IDs.Num();
	FGraph3DDelta graphDelta;
	TArray<FGraph3DDelta> deltas;
	int32 id = MOD_ID_NONE;

	switch (graphObjectType)
	{
	case EGraph3DObjectType::Vertex:
	{
		bValidDelta = (numPoints == 1) && TempVolumeGraph.GetDeltaForVertexAddition(points[0], graphDelta, NextID, id);
		deltas = { graphDelta };
	}
		break;
	case EGraph3DObjectType::Edge:
	{
		TArray<int32> OutEdgeIDs;
		if (numPoints == 2)
		{
			bValidDelta = TempVolumeGraph.GetDeltaForEdgeAdditionWithSplit(points[0], points[1], deltas, NextID, OutEdgeIDs, true);
		}
		else
		{
			bValidDelta = false;
		}
	}
		break;
	case EGraph3DObjectType::Face:
	{
		if (numPoints >= 3)
		{
			bValidDelta = TempVolumeGraph.GetDeltaForFaceAddition(points, deltas, NextID, id);
		}
	}
		break;
	default:
		break;
	}

	if (!bValidDelta)
	{
		// delta will be false if the object exists, out object ids should contain the existing id
		if (id != MOD_ID_NONE)
		{
			OutObjIDs.Add(id);
		}
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<int32> faceIDs, vertexIDs, edgeIDs;
	if (!ensureAlways(FinalizeGraphDeltas(deltas, faceIDs, vertexIDs, edgeIDs)))
	{
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);
		return false;
	}

	TArray<FDeltaPtr> deltaPtrs;
	for (auto& delta : deltas)
	{
		deltaPtrs.Add(MakeShared<FGraph3DDelta>(delta));
	}
	bool bSuccess = ApplyDeltas(deltaPtrs, world);

	OutObjIDs.Append(faceIDs);
	OutObjIDs.Append(vertexIDs);
	OutObjIDs.Append(edgeIDs);

	return bSuccess;
}

bool FModumateDocument::MakeScopeBoxObject(UWorld *world, const TArray<FVector> &points, TArray<int32> &OutObjIDs, const float Height)
{
	// TODO: reimplement
	return false;
}

bool FModumateDocument::FinalizeGraph2DDelta(const FGraph2DDelta &Delta, TMap<int32, FGraph2DHostedObjectDelta> &OutParentIDUpdates)
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

bool FModumateDocument::FinalizeGraphDelta(FGraph3D &TempGraph, FGraph3DDelta &Delta)
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

	TMap<int32, int32> childFaceIDToHostedID;
	TSet<int32> idsWithObjects;
	TSet<int32> rejectedObjects;

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
						int32 newObjID = NextID++;
						Delta.ParentIDUpdates.Add(newObjID, FGraph3DHostedObjectDelta(parentObj->GetChildIDs()[childIdx], MOD_ID_NONE, childFaceID));
						childFaceIDToHostedID.Add(childFaceID, newObjID);
						idsWithObjects.Add(childFaceID);
					}
					else
					{
						rejectedObjects.Add(parentObj->GetChildIDs()[childIdx]);
					}
				}

				// loop through children to find the new planes that they apply to
				// TODO: implementation is object specific and should be moved there
				// portals should be on 0 or 1 of the new planes, and finishes should be on all applicable planes
				for (int32 childObjId : childObj->GetChildIDs())
				{
					auto childHostedObj = GetObjectById(childObjId);
					if (childHostedObj && (childHostedObj->GetObjectType() == EObjectType::OTWindow || childHostedObj->GetObjectType() == EObjectType::OTDoor))
					{
						auto childFaceIDs = parentIDToChildrenIDs[parentID];

						TArray<FVector> corners;
						for (int32 corneridx = 0; corneridx < 4; corneridx++)
						{
							corners.Add(childHostedObj->GetCorner(corneridx));
						}
						TArray<FVector2D> portalCornersOnFace;
						for (FVector& corner : corners)
						{
							portalCornersOnFace.Add(parentFace->ProjectPosition2D(corner));
						}
						FBox2D portalBB = FBox2D(portalCornersOnFace);

						for (int32 childFaceID : childFaceIDs)
						{
							auto childFace = TempGraph.FindFace(childFaceID);
							// TODO: AABBs will not work for concave faces, and will even have problems for faces that aren't rectangles
							TArray<FVector2D> childCornersOnFace;
							for (FVector& childPos : childFace->CachedPositions)
							{
								childCornersOnFace.Add(parentFace->ProjectPosition2D(childPos));
							}
							// FBox2D functions are strict, so this adds tolerance
							FBox2D faceBB = FBox2D(childCornersOnFace).ExpandBy(VolumeGraph.Epsilon);

							if (faceBB.IsInside(portalBB))
							{
								Delta.ParentIDUpdates.Add(childObjId, FGraph3DHostedObjectDelta(MOD_ID_NONE, childObj->ID, childFaceIDToHostedID[childFaceID]));

								break;
							}
							// reject all cases where a portal would be split
							else 
							{
								// FBox2D functions are strict, so this adds tolerance
								faceBB = faceBB.ExpandBy(-VolumeGraph.Epsilon * 2.0f);
								if (faceBB.Intersect(portalBB))
								{
									return false;
								}
							}
						}
					}
					else if (childHostedObj && (childHostedObj->GetObjectType() == EObjectType::OTFinish))
					{
						auto childFaceIDs = parentIDToChildrenIDs[parentID];
						for (int32 childFaceID : childFaceIDs)
						{
							if (!rejectedObjects.Contains(childObj->ID))
							{
								int32 newObjID = NextID++;
								Delta.ParentIDUpdates.Add(newObjID, FGraph3DHostedObjectDelta(childObjId, MOD_ID_NONE, childFaceIDToHostedID[childFaceID]));
							}
						}
						Delta.ParentIDUpdates.Add(childObjId, FGraph3DHostedObjectDelta(MOD_ID_NONE, childObj->ID, MOD_ID_NONE));
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
			for (int32 childIdx = 0; childIdx < obj->GetChildIDs().Num(); childIdx++)
			{
				Delta.ParentIDUpdates.Add(obj->GetChildIDs()[childIdx], FGraph3DHostedObjectDelta(MOD_ID_NONE, objID, MOD_ID_NONE));
			}
		}
	}

	return true;
}

void FModumateDocument::ClearRedoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearRedoBuffer"));
	RedoBuffer.Empty();
}

void FModumateDocument::ClearUndoBuffer()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ClearUndoBuffer"));
	UndoBuffer.Empty();
	UndoRedoMacroStack.Empty();
}

FBoxSphereBounds FModumateDocument::CalculateProjectBounds() const
{
	TArray<FVector> allMOIPoints;
	TArray<FStructurePoint> curMOIPoints;
	TArray<FStructureLine> curMOILines;

	for (const FModumateObjectInstance *moi : ObjectInstanceArray)
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

bool FModumateDocument::CleanObjects(TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	static TArray<FModumateObjectInstance*> curDirtyList;
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
			TArray<FModumateObjectInstance*>& dirtyObjList = DirtyObjectMap.FindOrAdd(flagToClean);

			// Prevent iterating over objects dirtied with this flag too many times while trying to clean all objects
			int32 sameFlagSafeguard = 8;
			do
			{
				// Save off the current list of dirty objects, since it may change while cleaning them.
				bModifiedAnyObjects = false;
				curDirtyList = dirtyObjList;

				for (FModumateObjectInstance *objToClean : curDirtyList)
				{
					bModifiedAnyObjects = objToClean->CleanObject(flagToClean, OutSideEffectDeltas) || bModifiedAnyObjects;
					if (bModifiedAnyObjects)
					{
						++objectCleans;
					}
				}

			} while (bModifiedAnyObjects &&
				ensureMsgf(--sameFlagSafeguard > 0, TEXT("Infinite loop detected while cleaning objects with flag %s, breaking!"), *EnumValueString(EObjectDirtyFlags, flagToClean)));

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

void FModumateDocument::RegisterDirtyObject(EObjectDirtyFlags DirtyType, FModumateObjectInstance *DirtyObj, bool bDirty)
{
	// Make sure only one dirty flag is used at a time
	int32 flagInt = (int32)DirtyType;
	if (!ensureAlwaysMsgf((DirtyObj != nullptr) && (DirtyType != EObjectDirtyFlags::None) && ((flagInt & (flagInt - 1)) == 0),
		TEXT("Must mark a non-null object with exactly one dirty flag type at a time!")))
	{
		return;
	}

	TArray<FModumateObjectInstance*> &dirtyObjList = DirtyObjectMap.FindOrAdd(DirtyType);

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

void FModumateDocument::AddCommandToHistory(const FString &cmd)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::AddCommand"));
	CommandHistory.Add(cmd);
}

TArray<FString> FModumateDocument::GetCommandHistory() const
{
	return CommandHistory;
}

void FModumateDocument::MakeNew(UWorld *world)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::MakeNew"));

	int32 numObjects = ObjectInstanceArray.Num();
	for (int32 i = numObjects - 1; i >= 0; --i)
	{
		FModumateObjectInstance *obj = ObjectInstanceArray[i];

		ObjectsByID.Remove(obj->ID);
		ObjectInstanceArray.RemoveAt(i, 1, false);

		delete obj;
	}

	for (auto& kvp : DeletedObjects)
	{
		if (kvp.Value)
		{
			delete kvp.Value;
		}
	}
	DeletedObjects.Reset();

	for (auto &kvp : DirtyObjectMap)
	{
		kvp.Value.Reset();
	}

	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
	gameMode->ObjectDatabase->InitPresetManagerForNewDocument(PresetManager);
	
	// Clear drafting render directories
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;
	if (draftMan != nullptr)
	{
		draftMan->Reset();
	}

	NextID = 1;

	CommandHistory.Reset();

	ClearRedoBuffer();
	ClearUndoBuffer();

	UndoRedoMacroStack.Reset();
	VolumeGraph.Reset();
	TempVolumeGraph.Reset();
	SurfaceGraphs.Reset();

	GatherDocumentMetadata();

	static const FString eventCategory(TEXT("FileIO"));
	static const FString eventNameNew(TEXT("NewDocument"));
	UModumateAnalyticsStatics::RecordEventSimple(world, eventCategory, eventNameNew);

	SetCurrentProjectPath();
}


void FModumateDocument::GatherDocumentMetadata()
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::GatherDocumentMetadata"));
	FPartyProfile pp;

	// Read architect metadata from the registry
	pp.Role = TEXT("Architect (Author):");
	FString regSubKey = L"SOFTWARE\\Modumate\\OrganizationInfo\\";
	FString regValue = L"CompanyName";
	pp.Name = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);
	pp.Representative = TEXT("Andrew Stevenson");

	regValue = L"CompanyEmail";
	pp.Email = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);

	regValue = L"CompanyPhone";
	pp.Phone = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);

	regValue = L"CompanyLogoPath";
	FString relativeLogoPath = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);
	pp.LogoPath = FPaths::ProjectContentDir() + relativeLogoPath;

	LeadArchitect = pp;

	// Clear the secondary parties array
	SecondaryParties.Empty();

	pp.Role = TEXT("Structural Engineer");
	pp.Name = TEXT("ZFA Atructural Engineers");
	pp.Representative = TEXT("Frank Lloyd Wright");
	pp.Email = TEXT("franklloydwright@gmail.com");
	pp.Phone = TEXT("(702) 445-2314");
	SecondaryParties.Add(pp);

	pp.Role = TEXT("Mechanical Engineer");
	pp.Name = TEXT("Austal HVAC Design and Fabrication");
	pp.Representative = TEXT("Norman Foster");
	pp.Email = TEXT("normanfosetr@gmail.com");
	pp.Phone = TEXT("(408) 145-4315");
	SecondaryParties.Add(pp);

	pp.Role = TEXT("Interior Designer");
	pp.Name = TEXT("Frank Gehry Interiors");
	pp.Representative = TEXT("Frank Gehry");
	pp.Email = TEXT("frankgehry@gmail.com");
	pp.Phone = TEXT("(415) 220-4312");
	SecondaryParties.Add(pp);

	pp.Role = TEXT("Client:");
	pp.Name = TEXT("Andrew and Susan Stevenson");
	pp.Representative = TEXT("Johnny McClient");
	pp.Email = TEXT("johnnymcclient@gmail.com");
	pp.Phone = TEXT("(650) 350-3675");
	Client = pp;

	FDraftRevision dr;

	// Clear the revisions (versions) array
	Revisions.Empty();

	dr.Name = TEXT("First Revision. Subject to review and modifications, especially in the early stages of the project.");
	dr.DateTime = FDateTime::Now();
	dr.Number = 1;
	Revisions.Add(dr);

	dr.Name = TEXT("Moved All Of Frank's Lines.");
	dr.DateTime = FDateTime::Now();
	dr.Number = 2;
	Revisions.Add(dr);

	dr.Name = TEXT("Put Frank's Lines Back so that they can be accounted for when executing the project.");
	dr.DateTime = FDateTime::Now();
	dr.Number = 3;
	Revisions.Add(dr);

	regValue = L"LicenseStampPath";
	FString relativeStampPath = Modumate::PlatformFunctions::GetStringValueFromHKCU(regSubKey, regValue);
	StampPath = FPaths::ProjectContentDir() + relativeStampPath;

	ProjectInfo.name = TEXT("Stevenson Residence");
	ProjectInfo.address1 = TEXT("200 Holly Ave.");
	ProjectInfo.address2 = TEXT("San Francisco, CA 94116");
	ProjectInfo.lotNumber = TEXT("15-200-05");
	ProjectInfo.ID = TEXT("16097.001");
	ProjectInfo.description = TEXT("A 4-bedroom, 3-bathroom new transitional home with a pool and freestanding poolhouse/garage. The living room has a billiard table with a 6-chair bar and a wine cabinet.");
}

const FModumateObjectInstance *FModumateDocument::ObjectFromActor(const AActor *actor) const
{
	return const_cast<FModumateDocument*>(this)->ObjectFromActor(const_cast<AActor*>(actor));
}

FModumateObjectInstance *FModumateDocument::ObjectFromSingleActor(AActor *actor)
{
	auto *moiComponent = actor ? actor->FindComponentByClass<UModumateObjectComponent_CPP>() : nullptr;
	if (moiComponent && (moiComponent->ObjectID != MOD_ID_NONE))
	{
		return GetObjectById(moiComponent->ObjectID);
	}

	return nullptr;
}

FModumateObjectInstance *FModumateDocument::ObjectFromActor(AActor *actor)
{
	FModumateObjectInstance *moi = ObjectFromSingleActor(actor);

	while ((moi == nullptr) && (actor != nullptr))
	{
		actor = actor->GetAttachParentActor();
		moi = ObjectFromSingleActor(actor);
	}

	return moi;
}

TArray<FModumateObjectInstance*> FModumateDocument::GetObjectsOfType(EObjectType type)
{
	return ObjectInstanceArray.FilterByPredicate([type](FModumateObjectInstance *moi)
	{
		return moi->GetObjectType() == type;
	});
}

TArray<const FModumateObjectInstance*> FModumateDocument::GetObjectsOfType(const FObjectTypeSet& types) const
{
	TArray<const FModumateObjectInstance*> outArray;
	Algo::TransformIf(ObjectInstanceArray, outArray, [&types](FModumateObjectInstance * moi)
		{ return types.Contains(moi->GetObjectType()); },
		[](FModumateObjectInstance * moi) {return moi; });
	return outArray;
}

TArray<FModumateObjectInstance*> FModumateDocument::GetObjectsOfType(const FObjectTypeSet& types)
{
	return ObjectInstanceArray.FilterByPredicate([&types](FModumateObjectInstance *moi)
		{ return types.Contains(moi->GetObjectType()); });
}

void FModumateDocument::GetObjectIdsByAssembly(const FBIMKey& AssemblyKey, TArray<int32>& OutIds) const
{
	for (const auto &moi : ObjectInstanceArray)
	{
		if (moi->GetAssembly().UniqueKey() == AssemblyKey)
		{
			OutIds.Add(moi->ID);
		}
	}
}

TArray<const FModumateObjectInstance*> FModumateDocument::GetObjectsOfType(EObjectType type) const
{
//	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::GetObjectsOfType"));

	TArray< const FModumateObjectInstance *> outObjectsOfType;
	Algo::Transform(
			ObjectInstanceArray.FilterByPredicate([type](FModumateObjectInstance *moi)
			{
				return moi->GetObjectType() == type;
			}), 
		outObjectsOfType,
			[](FModumateObjectInstance *moi)
			{
				return moi;
			});
	return outObjectsOfType;
}

bool FModumateDocument::IsDirty() const
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::IsDirty"));
	return UndoBuffer.Num() > 0;
}


bool FModumateDocument::ExportPDF(UWorld *world, const TCHAR *filepath, const FVector &origin, const FVector &normal)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ExportPDF"));

	CurrentDraftingView = MakeShared<FModumateDraftingView>(world, this, FModumateDraftingView::kPDF);
	CurrentDraftingView->CurrentFilePath = FString(filepath);

	return true;
}

bool FModumateDocument::ExportDWG(UWorld * world, const TCHAR * filepath)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::ExportDWG"));
	CurrentDraftingView = MakeShared<FModumateDraftingView>(world, this, FModumateDraftingView::kDWG);
	CurrentDraftingView->CurrentFilePath = FString(filepath);

	return true;
}

bool FModumateDocument::Save(UWorld *world, const FString &path)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Save"));

	AEditModelGameMode_CPP *gameMode = Cast<AEditModelGameMode_CPP>(world->GetAuthGameMode());


	TSharedPtr<FJsonObject> FileJson = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> HeaderJson = MakeShared<FJsonObject>();

	// Header is its own
	FModumateDocumentHeader header;
	header.Version = Modumate::DocVersion;
	header.Thumbnail = CurrentEncodedThumbnail;
	FileJson->SetObjectField(DocHeaderField, FJsonObjectConverter::UStructToJsonObject<FModumateDocumentHeader>(header));

	FMOIDocumentRecord docRec;

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

	AEditModelPlayerController_CPP* emPlayerController = Cast<AEditModelPlayerController_CPP>(world->GetFirstPlayerController());
	AEditModelPlayerState_CPP* emPlayerState = emPlayerController->EMPlayerState;

	for (auto &mode : modes)
	{
		TScriptInterface<IEditModelToolInterface> tool = emPlayerController->ModeToTool.FindRef(mode);
		if (ensureAlways(tool))
		{
			docRec.CurrentToolAssemblyMap.Add(mode, tool->GetAssemblyKey().ToString());
		}
	}

	docRec.CommandHistory = CommandHistory;

	// DDL 2.0
	PresetManager.ToDocumentRecord(docRec);

	// Capture object instances into doc struct
	for (FModumateObjectInstance* obj : ObjectInstanceArray)
	{
		// Don't save graph-reflected MOIs, since their information is stored in separate graph structures
		EObjectType objectType = obj ? obj->GetObjectType() : EObjectType::OTNone;
		EGraph3DObjectType graph3DType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
		EGraphObjectType graph2DType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objectType);
		if (obj && (graph3DType == EGraph3DObjectType::None) && (graph2DType == EGraphObjectType::None))
		{
			FMOIStateData& stateData = obj->GetStateData();
			stateData.CustomData.SaveJsonFromCbor();
			docRec.ObjectData.Add(stateData);
		}
	}

	VolumeGraph.Save(&docRec.VolumeGraph);

	// Save all of the surface graphs as records
	for (const auto &kvp : SurfaceGraphs)
	{
		auto& surfaceGraph = kvp.Value;
		FGraph2DRecord& surfaceGraphRecord = docRec.SurfaceGraphs.Add(kvp.Key);
		surfaceGraph->ToDataRecord(&surfaceGraphRecord, true, true);
	}

	docRec.CameraViews = SavedCameraViews;

	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<FMOIDocumentRecord>(docRec);
	FileJson->SetObjectField(DocObjectInstanceField, docOb);

	FString ProjectJsonString;
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&ProjectJsonString);

	bool bWriteJsonSuccess = FJsonSerializer::Serialize(FileJson.ToSharedRef(), JsonStringWriter);
	if (!bWriteJsonSuccess)
	{
		return false;
	}

	bool fileSaveSuccess = FFileHelper::SaveStringToFile(ProjectJsonString, *path);

	if (fileSaveSuccess)
	{
		SetCurrentProjectPath(path);

		if (auto *gameInstance = world->GetGameInstance<UModumateGameInstance>())
		{
			gameInstance->UserSettings.RecordRecentProject(path, true);
		}
	}

	return fileSaveSuccess;
}

FModumateObjectInstance *FModumateDocument::GetObjectById(int32 id)
{
	return ObjectsByID.FindRef(id);
}

const FModumateObjectInstance *FModumateDocument::GetObjectById(int32 id) const
{
	return ObjectsByID.FindRef(id);
}

bool FModumateDocument::Load(UWorld *world, const FString &path, bool setAsCurrentProject)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDocument::Load"));

	//Get player state and tells it to empty selected object
	AEditModelPlayerController_CPP* EMPlayerController = Cast<AEditModelPlayerController_CPP>(world->GetFirstPlayerController());
	AEditModelPlayerState_CPP* EMPlayerState = EMPlayerController->EMPlayerState;
	EMPlayerState->OnNewModel();

	MakeNew(world);

	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();

	FModumateDatabase *objectDB = gameMode->ObjectDatabase;

	FModumateDocumentHeader docHeader;
	FMOIDocumentRecord docRec;

	if (FModumateSerializationStatics::TryReadModumateDocumentRecord(path, docHeader, docRec))
	{
		CommandHistory = docRec.CommandHistory;

		// Note: will check version against header and simply init from db if presets are out of date
		PresetManager.FromDocumentRecord(world, docHeader, docRec);

		// Load the connectivity graphs now, which contain associations between object IDs,
		// so that any objects whose geometry setup needs to know about connectivity can find it.
		VolumeGraph.Load(&docRec.VolumeGraph);
		FGraph3D::CloneFromGraph(TempVolumeGraph, VolumeGraph);

		// Load all of the surface graphs now
		for (const auto &kvp : docRec.SurfaceGraphs)
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
			// TODO: distinguish errors due to missing instance data vs. mismatched types or versions
			stateData.CustomData.SaveCborFromJson();
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
		for (auto *obj : ObjectInstanceArray)
		{
			if (EMPlayerState->DoesObjectHaveAnyError(obj->ID))
			{
				FString objectTypeString = EnumValueString(EObjectType, obj->GetObjectType());
				UE_LOG(LogTemp, Warning, TEXT("MOI %d (%s) has an error!"), obj->ID, *objectTypeString);
			}
		}

		for (auto &cta : docRec.CurrentToolAssemblyMap)
		{
			TScriptInterface<IEditModelToolInterface> tool = EMPlayerController->ModeToTool.FindRef(cta.Key);
			if (ensureAlways(tool))
			{
				const FBIMAssemblySpec *obAsm = PresetManager.GetAssemblyByKey(cta.Key, cta.Value);
				if (obAsm != nullptr)
				{
					tool->SetAssemblyKey(obAsm->UniqueKey());
				}
			}
		}

		// Just in case the loaded document's rooms don't match up with our current room analysis logic, do that now.
		UpdateRoomAnalysis(world);

		ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTWindow);
		ResequencePortalAssemblies_DEPRECATED(world, EObjectType::OTDoor);

		ClearUndoBuffer();

		if (setAsCurrentProject)
		{
			SetCurrentProjectPath(path);

			if (auto *gameInstance = world->GetGameInstance<UModumateGameInstance>())
			{
				gameInstance->UserSettings.RecordRecentProject(path, true);

				static const FString eventCategory(TEXT("FileIO"));
				static const FString eventName(TEXT("LoadDocument"));
				UModumateAnalyticsStatics::RecordEventSimple(world, eventCategory, eventName);
			}
		}
		return true;
	}
	return false;
}

TArray<int32> FModumateDocument::CloneObjects(UWorld *world, const TArray<int32> &objs, const FTransform& offsetTransform)
{
	// make a list of MOI objects from the IDs, send them to the MOI version of this function return array of IDs.

	TArray<FModumateObjectInstance*> obs;
	Algo::Transform(objs,obs,[this](auto id) {return GetObjectById(id); });

	obs = CloneObjects(
		world,
		obs,
		offsetTransform)
		.FilterByPredicate([](FModumateObjectInstance *ob) {return ob != nullptr; });

	TArray<int32> ids;

	Algo::Transform(obs,ids,[this](auto *ob){return ob->ID;});

	return ids;
}

int32 FModumateDocument::CloneObject(UWorld *world, const FModumateObjectInstance *original)
{
	// TODO: reimplement
	return MOD_ID_NONE;
}

TArray<FModumateObjectInstance *> FModumateDocument::CloneObjects(UWorld *world, const TArray<FModumateObjectInstance *> &objs, const FTransform& offsetTransform)
{
	TMap<FModumateObjectInstance*, FModumateObjectInstance*> oldToNew;

	BeginUndoRedoMacro();

	// Step 1 of clone macro: deserialize the objects, each of which will have its own undo/redo step
	TArray<FModumateObjectInstance*> newObjs;
	Algo::Transform(
		objs,
		newObjs,
		[this,world,&oldToNew,offsetTransform](FModumateObjectInstance *obj)
		{
			FModumateObjectInstance *newOb = GetObjectById(CloneObject(world, obj));
			oldToNew.Add(obj, newOb);
			return newOb;
		}
	);

	// Set up the correct parenting for the newly-cloned objects
	for (auto newObj : newObjs)
	{
		FModumateObjectInstance *oldParent = newObj->GetParentObject();
		FModumateObjectInstance *newParent = oldToNew.FindRef(oldParent);

		if (oldParent && newParent)
		{
			newObj->SetParentID(newParent->ID);
		}
	}

	EndUndoRedoMacro();

	return newObjs;
}

void FModumateDocument::SetCurrentProjectPath(const FString& currentProjectPath)
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

void FModumateDocument::UpdateMitering(UWorld *world, const TArray<int32> &dirtyObjIDs)
{
	TSet<int32> dirtyPlaneIDs;

	// Gather all of the dirty planes that need to be updated with new miter geometry
	for (int32 dirtyObjID : dirtyObjIDs)
	{
		FModumateObjectInstance *dirtyObj = GetObjectById(dirtyObjID);
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
				FModumateObjectInstance *parent = dirtyObj->GetParentObject();
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
		FModumateObjectInstance *dirtyPlane = GetObjectById(dirtyPlaneID);
		FGraph3DFace *dirtyFace = VolumeGraph.FindFace(dirtyPlaneID);
		if (dirtyPlane && dirtyFace)
		{
			const TArray<int32> &childIDs = dirtyPlane->GetChildIDs();
			if (childIDs.Num() == 1)
			{
				if (FModumateObjectInstance *dirtyPlaneHostedObj = GetObjectById(childIDs[0]))
				{
					dirtyPlaneHostedObj->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}
		}
	}
}

bool FModumateDocument::CanRoomContainFace(FGraphSignedID FaceID)
{
	const FGraph3DFace *graphFace = VolumeGraph.FindFace(FaceID);
	const FModumateObjectInstance *planeObj = GetObjectById(FMath::Abs(FaceID));

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
		const FModumateObjectInstance *planeChildObj = GetObjectById(planeChildID);
		if (planeChildObj && (planeChildObj->GetObjectType() == EObjectType::OTFloorSegment))
		{
			return true;
		}
	}

	return false;
}

void FModumateDocument::UpdateRoomAnalysis(UWorld *world)
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

				FModumateObjectInstance *oldRoomObj = GetObjectById(oldRoomObjID);
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
					FModumateObjectInstance *oldRoomObj = GetObjectById(oldRoomKVP.Key);
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
				FModumateObjectInstance *roomObj = GetObjectById(kvp.Key);
				const FString &newRoomNumber = kvp.Value;
				roomObj->SetProperty(EScope::Room, BIMPropertyNames::Number, newRoomNumber);
			}

			urSequenceRooms->Undo = [this, oldRoomNumbers]()
			{
				for (auto &kvp : oldRoomNumbers)
				{
					FModumateObjectInstance *roomObj = GetObjectById(kvp.Key);
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

const TSharedPtr<Modumate::FGraph2D> FModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID) const
{
	return SurfaceGraphs.FindRef(SurfaceGraphID);
}

TSharedPtr<Modumate::FGraph2D> FModumateDocument::FindSurfaceGraph(int32 SurfaceGraphID)
{
	return SurfaceGraphs.FindRef(SurfaceGraphID);
}

const TSharedPtr<Modumate::FGraph2D> FModumateDocument::FindSurfaceGraphByObjID(int32 ObjectID) const
{
	return const_cast<FModumateDocument*>(this)->FindSurfaceGraphByObjID(ObjectID);
}

TSharedPtr<Modumate::FGraph2D> FModumateDocument::FindSurfaceGraphByObjID(int32 ObjectID)
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

bool FModumateDocument::IsObjectInVolumeGraph(int32 ObjID, EGraph3DObjectType &OutObjType) const
{
	bool bIsInGraph = false;

	const FModumateObjectInstance *moi = GetObjectById(ObjID);
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

void FModumateDocument::DisplayDebugInfo(UWorld* world)
{
	auto displayMsg = [](const FString &msg)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, msg, false);
	};


	auto displayObjectCount = [this,displayMsg](EObjectType ot, const TCHAR *name)
	{
		TArray<FModumateObjectInstance*> obs = GetObjectsOfType(ot);
		if (obs.Num() > 0)
		{
			displayMsg(FString::Printf(TEXT("%s - %d"), name, obs.Num()));
		}
	};

	AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(world->GetFirstPlayerController()->PlayerState);

	displayMsg(TEXT("OBJECT COUNTS"));
	displayObjectCount(EObjectType::OTWallSegment, TEXT("OTWallSegment"));
	displayObjectCount(EObjectType::OTRailSegment, TEXT("OTRailSegment"));
	displayObjectCount(EObjectType::OTFloorSegment, TEXT("OTFloorSegment"));
	displayObjectCount(EObjectType::OTCeiling, TEXT("OTCeiling"));
	displayObjectCount(EObjectType::OTRoofFace, TEXT("OTRoof"));
	displayObjectCount(EObjectType::OTDoor, TEXT("OTDoor"));
	displayObjectCount(EObjectType::OTWindow, TEXT("OTWindow"));
	displayObjectCount(EObjectType::OTSystemPanel, TEXT("OTSystemPanel"));
	displayObjectCount(EObjectType::OTFurniture, TEXT("OTFurniture"));
	displayObjectCount(EObjectType::OTCabinet, TEXT("OTCabinet"));
	displayObjectCount(EObjectType::OTStaircase, TEXT("OTStaircase"));
	displayObjectCount(EObjectType::OTMullion, TEXT("OTMullion"));
	displayObjectCount(EObjectType::OTFinish, TEXT("OTFinish"));
	displayObjectCount(EObjectType::OTGroup, TEXT("OTGroup"));
	displayObjectCount(EObjectType::OTRoom, TEXT("OTRoom"));

	TSet<FBIMKey> asms;

	Algo::Transform(ObjectInstanceArray, asms,
		[](const FModumateObjectInstance *ob)
		{
			return ob->GetAssembly().UniqueKey();
		}
	);

	for (auto &a : asms)
	{
		int32 instanceCount = Algo::Accumulate(
			ObjectInstanceArray, 0,
			[a](int32 total, const FModumateObjectInstance *ob)
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

void FModumateDocument::DrawDebugVolumeGraph(UWorld* world)
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

		FString faceString = FString::Printf(TEXT("Face #%d: [%s]%s"), face.ID,
			*FString::JoinBy(face.EdgeIDs, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }),
			(face.GroupIDs.Num() == 0) ? TEXT("") : *FString::Printf(TEXT(" {%s}"), *FString::JoinBy(face.GroupIDs, TEXT(", "), [](const int32 &GroupID) { return FString::Printf(TEXT("%d"), GroupID); }))
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

void FModumateDocument::DrawDebugSurfaceGraphs(UWorld* world)
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
		const FModumateObjectInstance *surfaceGraphObj = GetObjectById(surfaceGraphID);
		const FModumateObjectInstance *surfaceGraphParent = surfaceGraphObj ? surfaceGraphObj->GetParentObject() : nullptr;
		int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceGraphObj);
		if (!ensure(surfaceGraphObj && surfaceGraphParent && (surfaceGraphFaceIndex != INDEX_NONE)))
		{
			continue;
		}

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

			FString faceString = FString::Printf(TEXT("Face #%d: [%s]"), poly.ID,
				*FString::JoinBy(poly.Edges, TEXT(", "), [](const FGraphSignedID &edgeID) { return FString::Printf(TEXT("%d"), edgeID); }));

			FVector2D originPos = poly.CachedPoints[0];
			FVector originDrawPos = UModumateGeometryStatics::Deproject2DPoint(originPos, faceAxisX, faceAxisY, faceOrigin);
			DrawDebugString(world, originDrawPos, faceString, nullptr, FColor::White, 0.0f, true);
		}
	}
}
