#include "ModumateCore/ModumateObjectDeltaStatics.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "DocumentManagement/ObjIDReservationHandle.h"
#include "Graph/Graph2DDelta.h"
#include "Graph/Graph3DTypes.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/CutPlane.h"
#include "Objects/FFE.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MetaEdge.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

void FModumateObjectDeltaStatics::GetTransformableIDs(const TArray<int32>& InObjectIDs, UModumateDocument *doc, TSet<int32>& OutTransformableIDs)
{
	for (int32 id : InObjectIDs)
	{
		auto moi = doc->GetObjectById(id);
		if (!ensureAlways(moi != nullptr))
		{
			continue;
		}
		EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType());

		if (const auto& graphObject = doc->GetVolumeGraph()->FindObject(id))
		{
			TArray<int32> vertexIDs;
			graphObject->GetVertexIDs(vertexIDs);
			OutTransformableIDs.Append(vertexIDs);
		}
		else if (auto parentGraphObject = doc->GetVolumeGraph()->FindObject(moi->GetParentID()))
		{
			TArray<int32> vertexIDs;
			parentGraphObject->GetVertexIDs(vertexIDs);
			OutTransformableIDs.Append(vertexIDs);
		}
		else if (auto surfaceGraph = doc->FindSurfaceGraphByObjID(id))
		{
			if (surfaceGraph == nullptr)
			{
				continue;
			}

			auto surfaceGraphObject = surfaceGraph->FindObject(id);
			if (surfaceGraphObject == nullptr)
			{
				surfaceGraphObject = surfaceGraph->FindObject(moi->GetParentID());
				if (surfaceGraphObject == nullptr)
				{
					continue;
				}
			}

			TArray<int32> vertexIDs;
			surfaceGraphObject->GetVertexIDs(vertexIDs);
			OutTransformableIDs.Append(vertexIDs);
		}
		else
		{
			OutTransformableIDs.Add(id);
		}
	}
}

bool FModumateObjectDeltaStatics::MoveTransformableIDs(const TMap<int32, FTransform>& ObjectMovements, UModumateDocument *doc, UWorld *World, bool bIsPreview)
{
	doc->ClearPreviewDeltas(World, bIsPreview);

	TMap<int32, FVector> vertex3DMovements;
	TMap<int32, TMap<int32, FVector2D>> combinedVertex2DMovements;
	TMap<int32, FTransform> nongraphMovements;

	const auto& graph = *doc->GetVolumeGraph();

	for (auto& kvp : ObjectMovements)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (!ensureAlways(moi != nullptr))
		{
			continue;
		}
		EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType());
		EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType());

		if (auto graphObject = graph.FindObject(moi->ID))
		{
			TArray<int32> vertexIDs;
			graphObject->GetVertexIDs(vertexIDs);

			if (!ensureAlways(vertexIDs.Num() == 1))
			{
				return false;
			}

			if (!vertex3DMovements.Contains(graphObject->ID))
			{
				vertex3DMovements.Add(graphObject->ID, kvp.Value.GetTranslation());
			}
		}
		else if (graph2DObjType != EGraphObjectType::None)
		{
			int32 targetParentID = moi->GetParentID();
			auto surfaceObj = doc->GetObjectById(targetParentID);
			auto surfaceGraph = doc->FindSurfaceGraph(targetParentID);
			auto surfaceParent = surfaceObj ? surfaceObj->GetParentObject() : nullptr;
			surfaceParent = surfaceParent ? surfaceParent : surfaceObj;
			if (!ensure(surfaceObj && surfaceGraph))
			{
				continue;
			}

			int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceObj);

			TArray<FVector> facePoints;
			FVector faceNormal, faceAxisX, faceAxisY;
			if (surfaceObj->GetObjectType() == EObjectType::OTTerrain)
			{   // Terrain has a simple basis:
				faceNormal = FVector::UpVector;
				faceAxisX = FVector::ForwardVector;
				faceAxisY = FVector::RightVector;
				facePoints.Add(surfaceObj->GetLocation());
			}
			else if (!ensure(UModumateObjectStatics::GetGeometryFromFaceIndex(surfaceParent, surfaceGraphFaceIndex, facePoints, faceNormal, faceAxisX, faceAxisY)))
			{
				continue;
			}
			FVector faceOrigin = facePoints[0];

			TMap<int32, FVector2D>& vertex2DMovements = combinedVertex2DMovements.FindOrAdd(targetParentID);
			if (auto surfaceGraphObject = surfaceGraph->FindObject(moi->ID))
			{
				TArray<int32> vertexIDs;
				surfaceGraphObject->GetVertexIDs(vertexIDs);
				if (!ensureAlways(vertexIDs.Num() == 1))
				{
					return false;
				}

				vertex2DMovements.Add(surfaceGraphObject->ID, UModumateGeometryStatics::ProjectPoint2D(kvp.Value.GetTranslation(), faceAxisX, faceAxisY, faceOrigin));
			}
		}
		else
		{
			nongraphMovements.Add(kvp);
		}
	}

	TArray<FDeltaPtr> deltas;
	if (vertex3DMovements.Num() > 0)
	{
		TArray<int32> vertexMoveIDs;
		TArray<FVector> vertexMovePositions;
		for (auto& kvp : vertex3DMovements)
		{
			vertexMoveIDs.Add(kvp.Key);
			vertexMovePositions.Add(kvp.Value);
		}

		if (bIsPreview ? 
			!doc->GetPreviewVertexMovementDeltas(vertexMoveIDs, vertexMovePositions, deltas) :
			!doc->GetVertexMovementDeltas(vertexMoveIDs, vertexMovePositions, deltas))
		{
			return false;
		}
	}
	else if (combinedVertex2DMovements.Num() > 0)
	{
		int32 nextID = doc->GetNextAvailableID();
		TArray<FGraph2DDelta> surfaceGraphDeltas;
		for (auto& kvp : combinedVertex2DMovements)
		{
			surfaceGraphDeltas.Reset();
			auto surfaceGraph = doc->FindSurfaceGraph(kvp.Key);
			if (!ensure(surfaceGraph.IsValid()) || (kvp.Value.Num() == 0))
			{
				continue;
			}

			if (bIsPreview ? 
				!surfaceGraph->MoveVerticesDirect(surfaceGraphDeltas, nextID, kvp.Value) :
				!surfaceGraph->MoveVertices(surfaceGraphDeltas, nextID, kvp.Value))
			{
				continue;
			}

			doc->FinalizeGraph2DDeltas(surfaceGraphDeltas, nextID, deltas);
		}
	}

	for (auto& kvp : nongraphMovements)
	{
		AModumateObjectInstance* moi = doc->GetObjectById(kvp.Key);
		FMOIDelta delta;
		auto& currentData = delta.AddMutationState(moi);
		if (moi->GetTransformedLocationState(kvp.Value, currentData))
		{
			deltas.Add(MakeShared<FMOIDelta>(delta));
		}
	}

	if (deltas.Num() > 0)
	{
		bIsPreview ? 
			doc->ApplyPreviewDeltas(deltas, World) :
			doc->ApplyDeltas(deltas, World);
	}
	else
	{
		return false;
	}

	return true;
}

void FModumateObjectDeltaStatics::SaveSelection(const TArray<int32>& InObjectIDs, UModumateDocument* doc, FMOIDocumentRecord* OutRecord)
{
	TSet<int32> volumeGraphIDs;
	TSet<int32> separatorIDs;
	TMap<int32, TSet<int32>> surfaceGraphIDToObjIDs;
	TSet<int32> ffeIDs;

	const auto& graph = *doc->GetVolumeGraph(); 

	for (int32 id : InObjectIDs)
	{
		auto moi = doc->GetObjectById(id);
		EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType());
		if (!ensureAlways(moi != nullptr))
		{
			continue;
		}

		if (auto graphObject = graph.FindObject(id))
		{
			volumeGraphIDs.Add(id);
		}
		else if (auto parentGraphObject = graph.FindObject(moi->GetParentID()))
		{
			volumeGraphIDs.Add(parentGraphObject->ID);
			separatorIDs.Add(moi->ID);
		}
		else if (auto surfaceGraph = doc->FindSurfaceGraphByObjID(id))
		{
			if (surfaceGraph == nullptr)
			{
				continue;
			}

			auto surfaceGraphObject = surfaceGraph->FindObject(id);
			if (surfaceGraphObject == nullptr)
			{
				surfaceGraphObject = surfaceGraph->FindObject(moi->GetParentID());
				if (surfaceGraphObject == nullptr)
				{
					continue;
				}
				separatorIDs.Add(id);
			}

			auto& surfaceGraphSubset = surfaceGraphIDToObjIDs.FindOrAdd(surfaceGraph->GetID());
			surfaceGraphSubset.Add(surfaceGraphObject->ID);
		}
		else
		{   // Everything else:
			ffeIDs.Add(id);
		}

	}

	graph.SaveSubset(volumeGraphIDs, &OutRecord->VolumeGraph);

	for (int32 separatorID : separatorIDs)
	{
		auto obj = doc->GetObjectById(separatorID);

		FMOIStateData& stateData = obj->GetStateData();
		stateData.CustomData.SaveJsonFromCbor();
		OutRecord->ObjectData.Add(stateData);
	}

	for (int32 ffeID : ffeIDs)
	{
		auto obj = doc->GetObjectById(ffeID);

		FMOIStateData& stateData = obj->GetStateData();
		stateData.CustomData.SaveJsonFromCbor();
		OutRecord->ObjectData.Add(stateData);
	}

	for (auto& kvp : surfaceGraphIDToObjIDs)
	{
		FGraph2DRecord currentRecord;
		auto surfaceGraph = doc->FindSurfaceGraph(kvp.Key);

		if (!surfaceGraph->SaveSubset(kvp.Value, &currentRecord))
		{
			continue;
		}

		auto obj = doc->GetObjectById(kvp.Key);

		FMOIStateData& stateData = obj->GetStateData();
		stateData.CustomData.SaveJsonFromCbor();
		OutRecord->ObjectData.Add(stateData);

		OutRecord->SurfaceGraphs.Add(kvp.Key, currentRecord);
	}
}

bool FModumateObjectDeltaStatics::PasteObjects(const FMOIDocumentRecord* InRecord, const FVector &InOrigin, UModumateDocument* doc, AEditModelPlayerController *Controller, bool bIsPreview)
{
	if (!InRecord)
	{
		return false;
	}

	const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	const FVector offset = hitLoc - InOrigin;

	auto World = Controller->GetWorld();
	TMap<int32, TArray<int32>> copiedToPastedObjIDs;

	if (bIsPreview)
	{
		doc->StartPreviewing();
	}

	if (InRecord->VolumeGraph.Vertices.Num() == 0 &&
		InRecord->VolumeGraph.Edges.Num() == 0 &&
		InRecord->VolumeGraph.Faces.Num() == 0 &&
		InRecord->SurfaceGraphs.Num() == 1)
	{
		FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		auto *cursorHitMOI = doc->ObjectFromActor(cursor.Actor);
		if (!cursorHitMOI)
		{
			return false;
		}
		
		TArray<FDeltaPtr> OutDeltas;
		{
			FObjIDReservationHandle objIDReservation(doc, cursorHitMOI->ID);
			int32& nextID = objIDReservation.NextID;
			if (!PasteObjectsWithinSurfaceGraph(InRecord, InOrigin, OutDeltas, doc, nextID, Controller, bIsPreview))
			{
				return false;
			}
		}
		
		bIsPreview ? 
			doc->ApplyPreviewDeltas(OutDeltas, Controller->GetWorld()) :
			doc->ApplyDeltas(OutDeltas, Controller->GetWorld());
		
		return true;
	}

	TArray<FDeltaPtr> OutDeltas;
	if (!doc->PasteMetaObjects(&InRecord->VolumeGraph, OutDeltas, copiedToPastedObjIDs, offset, bIsPreview))
	{
		return false;
	}

	// TODO: separate path for pure surface graph paste
	if (OutDeltas.Num() == 0)
	{
		return false;
	}

	int32 nextID = doc->GetNextAvailableID();

	// first pass - separators, surface graph base MOIs, other physical MOIs with parents.
	auto separatorDelta = MakeShared<FMOIDelta>();
	for(const auto& objRec: InRecord->ObjectData)
	{
		if (!copiedToPastedObjIDs.Contains(objRec.ParentID))
		{
			continue;
		}

		// TODO: surface graph splitting
		auto& newParents = copiedToPastedObjIDs[objRec.ParentID];
		if (objRec.ObjectType == EObjectType::OTSurfaceGraph && newParents.Num() > 1)
		{
			continue;
		}

		FMOIStateData stateData(objRec);
		const AModumateObjectInstance* sourceObj = doc->GetObjectById(objRec.ID);

		if (!ensure(sourceObj != nullptr))
		{
			continue;
		}

		// For MOIs that are parented and have their own transform.
		const FVector newLocation(sourceObj->GetLocation() + offset);
		sourceObj->GetTransformedLocationState({ sourceObj->GetRotation(), newLocation }, stateData);

		for (int32 parentID : newParents)
		{
			stateData.ID = nextID++;
			stateData.ParentID = parentID;

			separatorDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

			auto& pastedIDs = copiedToPastedObjIDs.FindOrAdd(objRec.ID);
			pastedIDs.Add(stateData.ID);
		}
	}

	OutDeltas.Add(separatorDelta);

	// the graphs need to exist, they may not at first
	for (auto& sgRec : InRecord->SurfaceGraphs)
	{
		if (!copiedToPastedObjIDs.Contains(sgRec.Key) || copiedToPastedObjIDs[sgRec.Key].Num() == 0)
		{
			continue;
		}

		auto tempGraph = MakeShared<FGraph2D>(copiedToPastedObjIDs[sgRec.Key][0]);
		TArray<FGraph2DDelta> sgDeltas;

		if (!tempGraph->PasteObjects(sgDeltas, nextID, &sgRec.Value, copiedToPastedObjIDs))
		{
			continue;
		}

		FGraph2DDelta addDelta(tempGraph->GetID());
		addDelta.DeltaType = EGraph2DDeltaType::Add;
		OutDeltas.Add(MakeShared<FGraph2DDelta>(addDelta));
			
		doc->FinalizeGraph2DDeltas(sgDeltas, nextID, OutDeltas);
	}

	// second pass - attachments
	auto attachmentDelta = MakeShared<FMOIDelta>();
	for(auto & objRec : InRecord->ObjectData)
	{
		// already done in separator pass
		if (copiedToPastedObjIDs.Contains(objRec.ID))
		{
			continue;
		}

		if (!copiedToPastedObjIDs.Contains(objRec.ParentID))
		{
			FMOIStateData stateData;
			const AModumateObjectInstance* sourceObj = doc->GetObjectById(objRec.ID);

			const FVector newLocation(sourceObj->GetLocation() + offset);
			if (sourceObj->GetTransformedLocationState({ sourceObj->GetRotation(), newLocation }, stateData))
			{
				stateData.ID = nextID++;
				attachmentDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

				auto& pastedIDs = copiedToPastedObjIDs.FindOrAdd(objRec.ID);
				pastedIDs.Add(stateData.ID);
			}
		}
		else
		{
			auto& newParents = copiedToPastedObjIDs[objRec.ParentID];
			for (int32 parentID : newParents)
			{
				FMOIStateData stateData;
				stateData.ID = nextID++;

				stateData.ObjectType = objRec.ObjectType;
				stateData.ParentID = parentID;
				stateData.AssemblyGUID = objRec.AssemblyGUID;
				stateData.CustomData = objRec.CustomData;

				attachmentDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

				auto& pastedIDs = copiedToPastedObjIDs.FindOrAdd(objRec.ID);
				pastedIDs.Add(stateData.ID);
			}
		}
	}

	OutDeltas.Add(attachmentDelta);

	bIsPreview ? 
		doc->ApplyPreviewDeltas(OutDeltas, World) :
		doc->ApplyDeltas(OutDeltas, World);


	return true;
}

bool FModumateObjectDeltaStatics::PasteObjectsWithinSurfaceGraph(const FMOIDocumentRecord* InRecord, const FVector& InOrigin, TArray<FDeltaPtr>& OutDeltas, UModumateDocument* doc, int32 &nextID, class AEditModelPlayerController* Controller, bool bIsPreview)
{
	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	auto *cursorHitMOI = doc->ObjectFromActor(cursor.Actor);
	if (!cursorHitMOI)
	{
		return false;
	}

	auto surfaceGraph = doc->FindSurfaceGraphByObjID(cursorHitMOI->ID);
	if (!surfaceGraph)
	{
		return false;
	}

	TMap<int32, TArray<int32>> copiedToPastedObjIDs;
	TArray<FGraph2DDelta> sgDeltas;

	auto iterator = InRecord->SurfaceGraphs.CreateConstIterator();
	FGraph2DRecord surfaceGraphRecord = iterator->Value;

	int32 originalID = iterator->Key;
	auto originalSurfaceGraphMoi = doc->GetObjectById(originalID);
	auto surfaceGraphMoi = doc->GetObjectById(surfaceGraph->GetID());
	if (!originalSurfaceGraphMoi || !surfaceGraphMoi)
	{
		return false;
	}

	FTransform originalTransform = FTransform(originalSurfaceGraphMoi->GetRotation(), originalSurfaceGraphMoi->GetLocation());
	FVector originalX = originalTransform.GetUnitAxis(EAxis::X);
	FVector originalY = originalTransform.GetUnitAxis(EAxis::Y);
	FVector2D origin = UModumateGeometryStatics::ProjectPoint2D(InOrigin, originalX, originalY, originalTransform.GetLocation());

	FTransform currentTransform = FTransform(surfaceGraphMoi->GetRotation(), surfaceGraphMoi->GetLocation());
	FVector currentX = currentTransform.GetUnitAxis(EAxis::X);
	FVector currentY = currentTransform.GetUnitAxis(EAxis::Y);
	FVector2D current = UModumateGeometryStatics::ProjectPoint2D(hitLoc, currentX, currentY, currentTransform.GetLocation());

	FVector2D offset = current - origin;

	if (!surfaceGraph->PasteObjects(sgDeltas, nextID, &surfaceGraphRecord, copiedToPastedObjIDs, bIsPreview, offset))
	{
		return false;
	}
	doc->FinalizeGraph2DDeltas(sgDeltas, nextID, OutDeltas);

	auto attachmentDelta = MakeShared<FMOIDelta>();
	for (auto& objRec : InRecord->ObjectData)
	{
		// already done in separator pass
		if (copiedToPastedObjIDs.Contains(objRec.ID))
		{
			continue;
		}

		if (!copiedToPastedObjIDs.Contains(objRec.ParentID))
		{
			continue;
		}

		auto& newParents = copiedToPastedObjIDs[objRec.ParentID];
		for (int32 parentID : newParents)
		{
			FMOIStateData stateData;
			stateData.ID = nextID++;

			stateData.ObjectType = objRec.ObjectType;
			stateData.ParentID = parentID;
			stateData.AssemblyGUID = objRec.AssemblyGUID;
			stateData.CustomData = objRec.CustomData;

			attachmentDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

			auto& pastedIDs = copiedToPastedObjIDs.FindOrAdd(objRec.ID);
			pastedIDs.Add(stateData.ID);
		}
	}

	OutDeltas.Add(attachmentDelta);


	return true;
}

// TODO: this code is derived from UInstPropWidgetEdgeDetail::CreateOrSwap, to be consolidated
bool FModumateObjectDeltaStatics::MakeSwapEdgeDetailDeltas(UModumateDocument *Doc, const TArray<uint32>& EdgeIDs,FGuid NewDetailPresetID, TArray<FDeltaPtr>& OutDeltas)
{
	auto document = Doc;

	bool bPopulatedDetailData = false;
	FEdgeDetailData newDetailData;
	const FBIMPresetInstance* swapPresetInstance = nullptr;
	FBIMPresetCollection& presetCollection = document->GetPresetCollection();
	bool bClearingDetail = !NewDetailPresetID.IsValid();

	if (NewDetailPresetID.IsValid())
	{
		swapPresetInstance = presetCollection.PresetFromGUID(NewDetailPresetID);

		// If we passed in a new preset ID, it better already be in the preset collection and have detail data.
		if (!ensure(swapPresetInstance && swapPresetInstance->TryGetCustomData(newDetailData)))
		{
			return false;
		}

		bPopulatedDetailData = true;
	}

	// make an edge detail on the selected edges that have no current detail, from the current conditions and naive mitering
	TMap<int32, int32> validDetailOrientationByEdgeID;
	TArray<int32> tempDetailOrientations;

	// For each edge that this detail widget is assigning a new detail to, make sure the details all have matching conditions,
	// and figure out which orientation to apply for each one.
	if (!bClearingDetail)
	{
		for (int32 edgeID : EdgeIDs)
		{
			auto edgeMOI = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));
			if (!ensure(edgeMOI))
			{
				return false;
			}

			if (bPopulatedDetailData)
			{
				FEdgeDetailData curDetailData(edgeMOI->GetMiterInterface());
				if (!ensure(newDetailData.CompareConditions(curDetailData, tempDetailOrientations) && (tempDetailOrientations.Num() > 0)))
				{
					return false;
				}

				validDetailOrientationByEdgeID.Add(edgeID, tempDetailOrientations[0]);
			}
			else
			{
				newDetailData.FillFromMiterNode(edgeMOI->GetMiterInterface());
				validDetailOrientationByEdgeID.Add(edgeID, 0);
			}
		}
	}

	// Now, create MOI delta(s) for creating/updating/deleting the edge detail MOI(s) for the selected edge(s)
	auto updateDetailMOIDelta = MakeShared<FMOIDelta>();
	int nextID = document->GetNextAvailableID();
	for (int32 edgeID : EdgeIDs)
	{
		auto edgeMOI = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));

		// If there already was a preset and the new one is empty, then only delete the edge detail MOI for each selected edge.
		if (bClearingDetail && ensure(edgeMOI && edgeMOI->CachedEdgeDetailMOI))
		{
			updateDetailMOIDelta->AddCreateDestroyState(edgeMOI->CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
			continue;
		}

		// Otherwise, update the state data and detail assembly for the edge.
		FMOIStateData newDetailState;
		if (edgeMOI->CachedEdgeDetailMOI)
		{
			newDetailState = edgeMOI->CachedEdgeDetailMOI->GetStateData();
		}
		else
		{
			newDetailState = FMOIStateData(nextID++, EObjectType::OTEdgeDetail, edgeID);
		}

		newDetailState.AssemblyGUID = NewDetailPresetID;
		newDetailState.CustomData.SaveStructData(FMOIEdgeDetailData(validDetailOrientationByEdgeID[edgeID]));

		if (edgeMOI->CachedEdgeDetailMOI)
		{
			updateDetailMOIDelta->AddMutationState(edgeMOI->CachedEdgeDetailMOI, edgeMOI->CachedEdgeDetailMOI->GetStateData(), newDetailState);
		}
		else
		{
			updateDetailMOIDelta->AddCreateDestroyState(newDetailState, EMOIDeltaType::Create);
		}
	}

	if (updateDetailMOIDelta->IsValid())
	{
		OutDeltas.Add(updateDetailMOIDelta);
	}

	return true;
}

void FModumateObjectDeltaStatics::ConvertGraphDeleteToMove(const TArray<FGraph3DDelta>& GraphDeltas, FGraph3D* OldGraph, int32& NextID, TArray<FGraph3DDelta>& OutDeltas)
{
	FGraph3D tempGraph;
	TArray<int32> newIDs;  // unused

	TMap<int32, int32> newFaceIDToOldFaceID;
	for (auto& graphDelta: GraphDeltas)
	{
		for (auto& edge : graphDelta.EdgeDeletions)
		{
			TArray<FGraph3DDelta> addEdgeDeltas;
			FVector vert0(OldGraph->FindVertex(edge.Value.Vertices[0])->Position);
			FVector vert1(OldGraph->FindVertex(edge.Value.Vertices[1])->Position);
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vert0, vert1, addEdgeDeltas, NextID, newIDs);
			for (auto& edgeDelta : addEdgeDeltas)
			{
				if (edgeDelta.EdgeAdditions.Num() == 1)
				{
					auto edgeDeltaCopy = *edgeDelta.EdgeAdditions.begin();
					edgeDelta.EdgeAdditions.Empty();
					edgeDelta.EdgeAdditions.Add(edge.Key) = MoveTemp(edgeDeltaCopy.Value);
					break;
				}
			}
			OutDeltas.Append(addEdgeDeltas);
		}

		for (auto& face: graphDelta.FaceDeletions)
		{
			const TArray<int32>& faceVertices = face.Value.Vertices;
			TArray<FVector> faceVertexPositions;
			for (int32 v: faceVertices)
			{
				faceVertexPositions.Add(OldGraph->FindVertex(v)->Position);
			}
			TArray<FGraph3DDelta> addFaceDeltas;
			tempGraph.GetDeltaForFaceAddition(faceVertexPositions, addFaceDeltas, NextID, newIDs);

			if (addFaceDeltas.Num() > 0 && addFaceDeltas[0].FaceAdditions.Num() == 1)
			{
				auto faceDelta = *addFaceDeltas[0].FaceAdditions.begin();
				newFaceIDToOldFaceID.Add(faceDelta.Key, face.Key);
				addFaceDeltas[0].FaceAdditions.Empty();
				addFaceDeltas[0].FaceAdditions.Add(face.Key) = MoveTemp(faceDelta.Value);
			}

			// Handle face IDs in any containment updates:
			for (auto& addFaceDelta : addFaceDeltas)
			{
				if (addFaceDelta.FaceContainmentUpdates.Num() > 0)
				{
					TMap<int32, FGraph3DFaceContainmentDelta> newContainmentUpdates;

					for (auto& faceContainmentUpdate : addFaceDelta.FaceContainmentUpdates)
					{
						auto& newContainment = newContainmentUpdates.Add(newFaceIDToOldFaceID[faceContainmentUpdate.Key]);
						newContainment.NextContainingFaceID = 
							faceContainmentUpdate.Value.NextContainingFaceID == MOD_ID_NONE ? MOD_ID_NONE : newFaceIDToOldFaceID[faceContainmentUpdate.Value.NextContainingFaceID];
						newContainment.PrevContainingFaceID = 
							faceContainmentUpdate.Value.PrevContainingFaceID == MOD_ID_NONE ? MOD_ID_NONE : newFaceIDToOldFaceID[faceContainmentUpdate.Value.PrevContainingFaceID];

						for (int32 newfaceID : faceContainmentUpdate.Value.ContainedFaceIDsToAdd)
						{
							newContainment.ContainedFaceIDsToAdd.Add(newFaceIDToOldFaceID[newfaceID]);
						}
					}

					addFaceDelta.FaceContainmentUpdates = MoveTemp(newContainmentUpdates);
				}
			}

			OutDeltas.Append(addFaceDeltas);
		}
	}
}
