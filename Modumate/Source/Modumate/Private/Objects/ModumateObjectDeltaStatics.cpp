#include "Objects/ModumateObjectDeltaStatics.h"

#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateSerialization.h"
#include "DocumentManagement/ObjIDReservationHandle.h"
#include "Graph/Graph2DDelta.h"
#include "Graph/Graph3DTypes.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/CutPlane.h"
#include "Objects/FFE.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MetaEdge.h"
#include "Objects/MetaGraph.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/MetaPlaneSpan.h"

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

		const FGraph3D* graph3d = doc->FindVolumeGraph(id);
		if (const auto& graphObject = graph3d ? graph3d->FindObject(id) : nullptr)
		{
			TArray<int32> vertexIDs;
			graphObject->GetVertexIDs(vertexIDs);
			OutTransformableIDs.Append(vertexIDs);
		}
		else
		{
			graph3d = doc->FindVolumeGraph(moi->GetParentID());
			if (auto parentGraphObject = graph3d ? graph3d->FindObject(moi->GetParentID()) : nullptr)
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
			else if (moi->GetObjectType() == EObjectType::OTMetaGraph)
			{
				TArray<int32> allVertexIDs;
				doc->GetVolumeGraph(id)->GetVertices().GenerateKeyArray(allVertexIDs);
				OutTransformableIDs.Append(allVertexIDs);
			}
			else if (moi->GetParentObject() && 
					(moi->GetParentObject()->GetObjectType() == EObjectType::OTMetaEdgeSpan ||
					moi->GetParentObject()->GetObjectType() == EObjectType::OTMetaPlaneSpan))
			{
				TArray<int32> spanMemberIDs;
				AMOIMetaEdgeSpan* edgeSpan = Cast<AMOIMetaEdgeSpan>(moi->GetParentObject());
				if (edgeSpan)
				{
					spanMemberIDs = edgeSpan->InstanceData.GraphMembers;
				}
				else
				{
					AMOIMetaPlaneSpan* planeSpan = Cast<AMOIMetaPlaneSpan>(moi->GetParentObject());
					if (planeSpan)
					{
						spanMemberIDs = planeSpan->InstanceData.GraphMembers;
					}
				}
				for (auto curGraphMemberID : spanMemberIDs)
				{
					graph3d = doc->FindVolumeGraph(curGraphMemberID);
					if (auto curGraphMemberObj = graph3d ? graph3d->FindObject(curGraphMemberID) : nullptr)
					{
						TArray<int32> vertexIDs;
						curGraphMemberObj->GetVertexIDs(vertexIDs);
						OutTransformableIDs.Append(vertexIDs);
					}
				}
			}
			else
			{
				OutTransformableIDs.Add(id);
			}
		}
	}
}

bool FModumateObjectDeltaStatics::MoveTransformableIDs(const TMap<int32, FTransform>& ObjectMovements, UModumateDocument *doc, UWorld *World, bool bIsPreview,
	const TArray<FDeltaPtr>* AdditionalDeltas /*= nullptr*/)
{
	doc->ClearPreviewDeltas(World, bIsPreview);

	TMap<int32, FVector> vertex3DMovements;
	TMap<int32, TMap<int32, FVector2D>> combinedVertex2DMovements;
	TMap<int32, FTransform> nongraphMovements;

	for (auto& kvp : ObjectMovements)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (!ensureAlways(moi != nullptr))
		{
			continue;
		}
		EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType());
		EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType());

		const auto* graph = doc->FindVolumeGraph(moi->ID);
		if (auto graphObject = graph ? graph->FindObject(moi->ID) : nullptr)
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
	if (AdditionalDeltas)
	{
		deltas.Append(*AdditionalDeltas);
	}

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
		else if (UModumateTypeStatics::IsSpanObject(moi->GetParentObject()))
		{
			auto* spanObject = moi->GetParentObject();
			volumeGraphIDs.Append(spanObject->GetObjectType() == EObjectType::OTMetaEdgeSpan ?
				Cast<AMOIMetaEdgeSpan>(spanObject)->InstanceData.GraphMembers :
				Cast<AMOIMetaPlaneSpan>(spanObject)->InstanceData.GraphMembers);
			separatorIDs.Add(spanObject->ID);
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

bool FModumateObjectDeltaStatics::PasteObjects(const FMOIDocumentRecord* InRecord, const FVector& InOrigin, UModumateDocument* Doc, const AEditModelPlayerController* Controller,
	bool bIsPreview, const TArray<FDeltaPtr>* AdditionalDeltas /*= nullptr*/)
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
		Doc->StartPreviewing();
	}

	if (InRecord->VolumeGraph.Vertices.Num() == 0 &&
		InRecord->VolumeGraph.Edges.Num() == 0 &&
		InRecord->VolumeGraph.Faces.Num() == 0 &&
		InRecord->SurfaceGraphs.Num() == 1)
	{
		FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		auto *cursorHitMOI = Doc->ObjectFromActor(cursor.Actor);
		if (!cursorHitMOI)
		{
			return false;
		}
		
		TArray<FDeltaPtr> OutDeltas;
		if (AdditionalDeltas)
		{
			OutDeltas.Append(*AdditionalDeltas);
		}
		{
			FObjIDReservationHandle objIDReservation(Doc, cursorHitMOI->ID);
			int32& nextID = objIDReservation.NextID;
			if (!PasteObjectsWithinSurfaceGraph(InRecord, InOrigin, OutDeltas, Doc, nextID, Controller, bIsPreview))
			{
				return false;
			}
		}
		
		bIsPreview ? 
			Doc->ApplyPreviewDeltas(OutDeltas, Controller->GetWorld()) :
			Doc->ApplyDeltas(OutDeltas, Controller->GetWorld());
		
		return true;
	}

	TArray<FDeltaPtr> OutDeltas;
	if (AdditionalDeltas)
	{
		OutDeltas.Append(*AdditionalDeltas);
	}
	if (!Doc->PasteMetaObjects(&InRecord->VolumeGraph, OutDeltas, copiedToPastedObjIDs, offset, bIsPreview))
	{
		return false;
	}

	// TODO: separate path for pure surface graph paste
	if (OutDeltas.Num() == 0)
	{
		return false;
	}

	int32 nextID = Doc->GetNextAvailableID();

	// first pass - separators, surface graph base MOIs, other physical MOIs with parents.
	auto separatorDelta = MakeShared<FMOIDelta>();
	for(const auto& objRec: InRecord->ObjectData)
	{
		FMOIStateData stateData(objRec);
		const AModumateObjectInstance* sourceObj = Doc->GetObjectById(objRec.ID);
		if (!ensure(sourceObj != nullptr))
		{
			continue;
		}


		if (UModumateTypeStatics::IsSpanObject(sourceObj))
		{
			TArray<int32> newGraphMembers;
			const TArray<int32>& currentGraphMembers = objRec.ObjectType == EObjectType::OTMetaEdgeSpan
				? Cast<const AMOIMetaEdgeSpan>(sourceObj)->InstanceData.GraphMembers :
				Cast<const AMOIMetaPlaneSpan>(sourceObj)->InstanceData.GraphMembers;

			for (int32 gm : currentGraphMembers)
			{
				if (copiedToPastedObjIDs.Contains(gm))
				{	// TODO: This may fail due to overlapping copying.
					newGraphMembers.Append(copiedToPastedObjIDs[gm]);
				}
			}
			if (newGraphMembers.Num() == 0)
			{
				continue;
			}

			if (objRec.ObjectType == EObjectType::OTMetaEdgeSpan)
			{
				FMOIMetaEdgeSpanData spanData;
				stateData.CustomData.LoadStructData(spanData);
				spanData.GraphMembers = MoveTemp(newGraphMembers);
				stateData.CustomData.SaveStructData(spanData);
			}
			else
			{
				FMOIMetaPlaneSpanData spanData;
				stateData.CustomData.LoadStructData(spanData);
				spanData.GraphMembers = MoveTemp(newGraphMembers);
				stateData.CustomData.SaveStructData(spanData);
			}

			stateData.ID = nextID++;
			copiedToPastedObjIDs.Add(objRec.ID, { stateData.ID });
			separatorDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
			continue;
		}

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
			
		Doc->FinalizeGraph2DDeltas(sgDeltas, nextID, OutDeltas);
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
			const AModumateObjectInstance* sourceObj = Doc->GetObjectById(objRec.ID);

			if (!ensure(sourceObj != nullptr))
			{
				continue;
			}

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
		Doc->ApplyPreviewDeltas(OutDeltas, World) :
		Doc->ApplyDeltas(OutDeltas, World);


	return true;
}

bool FModumateObjectDeltaStatics::PasteObjectsWithinSurfaceGraph(const FMOIDocumentRecord* InRecord, const FVector& InOrigin, TArray<FDeltaPtr>& OutDeltas, UModumateDocument* doc,
	int32 &nextID, const AEditModelPlayerController* Controller, bool bIsPreview)
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

void FModumateObjectDeltaStatics::ConvertGraphDeleteToMove(const TArray<FGraph3DDelta>& GraphDeltas, const FGraph3D* OldGraph, int32& NextID, TArray<FGraph3DDelta>& OutDeltas)
{
	FGraph3D tempGraph;
	TArray<int32> newIDs;  // unused

	TMap<int32, int32> newFaceIDToOldFaceID;
	newFaceIDToOldFaceID.Add(MOD_ID_NONE, MOD_ID_NONE);

	for (auto& graphDelta: GraphDeltas)
	{
		FGraph3DDelta vertexDelta;
		for (auto& vertex : graphDelta.VertexDeletions)
		{
			int32 vertID = vertex.Key;
			int32 outID;  // unused
			tempGraph.GetDeltaForVertexAddition(vertex.Value, vertexDelta, vertID, outID);
		}

		if (vertexDelta.VertexAdditions.Num() > 0)
		{
			// Creating delta doesn't apply it to graph, unlike edges/faces. Not sure why.
			tempGraph.ApplyDelta(vertexDelta);
			OutDeltas.Add({ vertexDelta });
		}

		for (auto& edge : graphDelta.EdgeDeletions)
		{
			TArray<FGraph3DDelta> addEdgeDeltas;
			FVector vert0(OldGraph->FindVertex(edge.Value.Vertices[0])->Position);
			FVector vert1(OldGraph->FindVertex(edge.Value.Vertices[1])->Position);
			tempGraph.GetDeltaForEdgeAdditionWithSplit(vert0, vert1, addEdgeDeltas, NextID, newIDs, false /* bCheckFaces*/, true /* bSplitAndUpdateEdges */);
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
			tempGraph.GetDeltaForFaceAddition(faceVertexPositions, addFaceDeltas, NextID, newIDs, true /* bSplitAndUpdateFaces */);

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
						newContainment.NextContainingFaceID = newFaceIDToOldFaceID[faceContainmentUpdate.Value.NextContainingFaceID];
						newContainment.PrevContainingFaceID = newFaceIDToOldFaceID[faceContainmentUpdate.Value.PrevContainingFaceID];

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

void FModumateObjectDeltaStatics::DuplicateGroups(const UModumateDocument* Doc, const TSet<int32>& GroupIDs, int32& NextID, TArray<TPair<bool, FDeltaPtr>>& OutDeltas)
{
	TMap<int32, int32> newGroupIDMap;

	// Create new group IDs first to be able to preserve hierarchy.
	for (int32 id : GroupIDs)
	{
		newGroupIDMap.Add(id, NextID++);
	}

	for (int32 groupID: GroupIDs)
	{
		const FGraph3D* graph = Doc->GetVolumeGraph(groupID);
		if (!ensure(graph))
		{
			return;
		}

		const AModumateObjectInstance* graphObject = Doc->GetObjectById(groupID);
		if (!ensure(graphObject))
		{
			return;
		}

		int32 parentID = graphObject->GetParentID();
		if (parentID == MOD_ID_NONE)
		{   // Can't duplicate root group
			return;
		}

		int32 newGroupID = newGroupIDMap[groupID];
		if (newGroupIDMap.Contains(parentID))
		{
			parentID = newGroupIDMap[parentID];
		}

		FMOIStateData stateData(newGroupID, EObjectType::OTMetaGraph, parentID);
		stateData.CustomData.SaveStructData<FMOIMetaGraphData>(FMOIMetaGraphData());

		auto delta = MakeShared<FMOIDelta>();
		delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
		OutDeltas.Emplace(false, delta);

		auto addGraph = MakeShared<FGraph3DDelta>();
		addGraph->DeltaType = EGraph3DDeltaType::Add;
		addGraph->GraphID = newGroupID;
		OutDeltas.Emplace(false, addGraph);

		TMap<int32, int32> oldIDToNewID;
		oldIDToNewID.Add(MOD_ID_NONE, MOD_ID_NONE);

		auto newElementsDelta = MakeShared<FGraph3DDelta>();
		newElementsDelta->GraphID = newGroupID;
		const auto& allVerts = graph->GetVertices();
		const auto& allEdges = graph->GetEdges();
		const auto& allFaces = graph->GetFaces();
		for (const auto& kvp : allVerts)
		{
			newElementsDelta->VertexAdditions.Add(NextID, kvp.Value.Position);
			oldIDToNewID.Add(kvp.Key, NextID);
			++NextID;
		}
		for (const auto& kvp : allEdges)
		{
			FGraph3DObjDelta newEdge(FGraphVertexPair(oldIDToNewID[kvp.Value.StartVertexID], oldIDToNewID[kvp.Value.EndVertexID]), TArray<int32>());
			newElementsDelta->EdgeAdditions.Add(NextID, newEdge);
			oldIDToNewID.Add(kvp.Key, NextID);
			++NextID;
		}
		for (const auto& kvp : allFaces)
		{
			TArray<int32> newVertices;
			for (int32 v : kvp.Value.VertexIDs)
			{
				newVertices.Add(oldIDToNewID[v]);
			}
			FGraph3DObjDelta newFace(newVertices, TArray<int32>(), kvp.Value.ContainingFaceID, kvp.Value.ContainedFaceIDs);
			newElementsDelta->FaceAdditions.Add(NextID, newFace);
			oldIDToNewID.Add(kvp.Key, NextID);
			++NextID;
		}

		// Map old contained/containing face IDs:
		for (auto& kvp : newElementsDelta->FaceAdditions)
		{
			kvp.Value.ContainingObjID = oldIDToNewID[kvp.Value.ContainingObjID];
			TSet<int32> containedFaces;
			for (int32 oldVert : kvp.Value.ContainedObjIDs)
			{
				containedFaces.Add(oldIDToNewID[oldVert]);
			}
			kvp.Value.ContainedObjIDs = containedFaces;
		}

		OutDeltas.Emplace(true, newElementsDelta);

		auto moiDelta = MakeShared<FMOIDelta>();

		TArray<FDeltaPtr> objectDeltas;
		for (const auto& moiKvp : graph->GetAllObjects())
		{
			int32 moiID = moiKvp.Key;
			TArray<const AModumateObjectInstance*> objects;
			objects.Add(Doc->GetObjectById(moiID));
			UModumateObjectStatics::GetAllDescendents(objects[0], objects);
			for (const auto* object : objects)
			{
				EObjectType objectType = object->GetObjectType();
				if (UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType) == EGraph3DObjectType::None)
				{
					FMOIStateData newState(object->GetStateData());
					oldIDToNewID.Add(newState.ID, NextID);
					newState.ID = NextID;
					++NextID;

					// Duplicate span objects mapping graph IDs:
					switch (objectType)
					{
					case EObjectType::OTMetaEdgeSpan:
					{
						FMOIMetaEdgeSpanData instanceData(Cast<const AMOIMetaEdgeSpan>(object)->InstanceData);
						Algo::ForEach(instanceData.GraphMembers, [&oldIDToNewID](int32& S)
							{ S = oldIDToNewID[S]; });
						newState.CustomData.SaveStructData(instanceData);
						break;
					}

					case EObjectType::OTMetaPlaneSpan:
					{
						FMOIMetaPlaneSpanData instanceData(Cast<const AMOIMetaPlaneSpan>(object)->InstanceData);
						Algo::ForEach(instanceData.GraphMembers, [&oldIDToNewID](int32& S)
							{ S = oldIDToNewID[S]; });
						newState.CustomData.SaveStructData(instanceData);
						break;
					}

					default:
						break;
					}

					if (ensure(oldIDToNewID.Contains(newState.ParentID)))
					{
						newState.ParentID = oldIDToNewID[newState.ParentID];
						moiDelta->AddCreateDestroyState(newState, EMOIDeltaType::Create);
					}
				}
			}
		}

		OutDeltas.Emplace(false, moiDelta);
	}
}

void FModumateObjectDeltaStatics::MergeGraphToCurrentGraph(UModumateDocument* Doc, const FGraph3D* OldGraph, int32& NextID, TArray<FDeltaPtr>& OutDeltas, TSet<int32>& OutItemsForSelection)
{
	FGraph3DRecord graphRecord;
	OldGraph->Save(&graphRecord);
	TMap<int32, TArray<int32>> copiedToPastedIDs;
	int32 localID;  // unused
	int32 myPlayerIdx;
	Doc->SplitMPObjID(NextID, localID, myPlayerIdx);

	Doc->SetNextID(NextID, MOD_ID_NONE);

	Doc->PasteMetaObjects(&graphRecord, OutDeltas, copiedToPastedIDs, FVector::ZeroVector, false);

	auto delta = MakeShared<FMOIDelta>();
	for (const auto& kvp: copiedToPastedIDs)
	{
		const AModumateObjectInstance* graphMoi = Doc->GetObjectById(kvp.Key);
		if (ensureAlways(graphMoi && kvp.Value.Num() > 0))
		{
			TArray<const AModumateObjectInstance*> childObjects = graphMoi->GetChildObjects();
			TArray<int32> spanIDs;
			UModumateObjectStatics::GetSpansForEdgeObject(Doc, graphMoi, spanIDs);
			UModumateObjectStatics::GetSpansForFaceObject(Doc, graphMoi, spanIDs);
			for (int32 spanID: spanIDs)
			{
				childObjects.Add(Doc->GetObjectById(spanID));
			}

			if (childObjects.Num() == 0)
			{
				OutItemsForSelection.Add(kvp.Value[0]);
			}

			for (const auto* childMoi: childObjects)
			{
				if (UModumateTypeStatics::IsSpanObject(childMoi))
				{
					FModumateObjectDeltaStatics::GetDeltaForSpanMapping(childMoi, copiedToPastedIDs, OutDeltas);
					OutItemsForSelection.Append(childMoi->GetChildIDs());
				}
				else
				{
					OutItemsForSelection.Add(childMoi->ID);
					int32 newParentID = kvp.Value[0];
					if (kvp.Value.Num() > 1)
					{
						UE_LOG(LogTemp, Warning, TEXT("Graph element %d was split into %d new elements"), kvp.Key, kvp.Value.Num());
					}

					const FMOIStateData& oldstateData = childMoi->GetStateData();
					FMOIStateData newStateData(oldstateData);
					newStateData.ParentID = newParentID;
					delta->AddMutationState(childMoi, oldstateData, newStateData);
				}
			}

			// Check if locally-generated ID and bump NextID if appropriate.
			int32 playerIdx;
			for (int32 id : kvp.Value)
			{
				Doc->SplitMPObjID(id, localID, playerIdx);
				if (playerIdx == myPlayerIdx)
				{
					NextID = FMath::Max(NextID, id + 1);
				}
			}

		}
	}

	if (delta->IsValid())
	{
		OutDeltas.Add(delta);
	}

}

void FModumateObjectDeltaStatics::GetDeltasForGroupTransforms(UModumateDocument* Doc, const TMap<int32, FVector>& OriginalGroupVertexTranslations, const FTransform transform, TArray<FDeltaPtr>& OutDeltas)
{
	for (const auto& kvp : OriginalGroupVertexTranslations)
	{
		int id = kvp.Key;
		int32 graphId = Doc->FindGraph3DByObjID(id);
		FGraph3D* graph = Doc->GetVolumeGraph(graphId);
		const FGraph3DVertex* vertex = graph ? graph->GetVertices().Find(id) : nullptr;
		if (ensure(vertex))
		{
			FGraph3DDelta delta(graphId);
			FVector position = kvp.Value;
			delta.VertexMovements.Add(id, FModumateVectorPair(position, transform.TransformPositionNoScale(position)));
			OutDeltas.Add(MakeShared<FGraph3DDelta>(delta));
		}
	}
}

void FModumateObjectDeltaStatics::GetDeltasForGraphDelete(UModumateDocument* Doc, int32 GraphID, TArray<FDeltaPtr>& OutDeltas)
{
	auto* graph = Doc->GetVolumeGraph(GraphID);
	if (graph)
	{
		auto& graphElements = graph->GetAllObjects();
		TSharedPtr<FMOIDelta> moisDelta = MakeShared<FMOIDelta>();
		TSharedPtr<FGraph3DDelta> graphDelta = MakeShared<FGraph3DDelta>(GraphID);

		for (auto& kvp : graphElements)
		{
			int32 id = kvp.Key;
			switch (kvp.Value)
			{
			case EGraph3DObjectType::Vertex:
				graphDelta->VertexDeletions.Add(id, graph->GetVertices()[id].Position);
				break;

			case EGraph3DObjectType::Edge:
			{
				TArray<int32> edgeVerts;
				graph->GetEdges()[id].GetVertexIDs(edgeVerts);
				graphDelta->EdgeDeletions.Add(id, FGraph3DObjDelta(edgeVerts));
				break;
			}

			case EGraph3DObjectType::Face:
			{
				const FGraph3DFace& face = graph->GetFaces()[id];
				graphDelta->FaceDeletions.Add(id, FGraph3DObjDelta(face.VertexIDs, TArray<int32>(), face.ContainingFaceID, face.ContainedFaceIDs));
				break;
			}
			}

			auto* metaObject = Doc->GetObjectById(id);
			if (ensure(metaObject))
			{
				TArray<const AModumateObjectInstance*> mois;
				UModumateObjectStatics::GetAllDescendents(metaObject, mois);
				for (auto* moi : mois)
				{
					moisDelta->AddCreateDestroyState(moi->GetStateData(), EMOIDeltaType::Destroy);
				}
			}
		}

		if (moisDelta->IsValid())
		{
			OutDeltas.Add(moisDelta);
		}
		if (!graphDelta->IsEmpty())
		{
			OutDeltas.Add(graphDelta);

			TSharedPtr<FGraph3DDelta> deleteGraph = MakeShared<FGraph3DDelta>(GraphID);
			deleteGraph->DeltaType = EGraph3DDeltaType::Remove;
			OutDeltas.Add(deleteGraph);
			TSharedPtr<FMOIDelta> deleteMetaGraph = MakeShared<FMOIDelta>();
			deleteMetaGraph->AddCreateDestroyState(Doc->GetObjectById(GraphID)->GetStateData(), EMOIDeltaType::Destroy);
			OutDeltas.Add(deleteMetaGraph);
		}
	}
}

void FModumateObjectDeltaStatics::GetDeltaForSpanMapping(const AModumateObjectInstance* Moi, const TMap<int32, TArray<int32>>& CopiedToPastedObjIDs, TArray<FDeltaPtr>& OutDeltas)
{
	if (Moi == nullptr)
	{
		return;
	}

	auto newSpanDelta = MakeShared<FMOIDelta>();
	FMOIStateData stateData(Moi->GetStateData());

	switch (Moi->GetObjectType())
	{
	case EObjectType::OTMetaEdgeSpan:
	{
		FMOIMetaEdgeSpanData spanData;
		Moi->GetStateData().CustomData.LoadStructData(spanData);
		spanData.GraphMembers.Empty();
		for (int32 spanMember : Cast<AMOIMetaEdgeSpan>(Moi)->InstanceData.GraphMembers)
		{
			const auto* pastedIDs = CopiedToPastedObjIDs.Find(spanMember);
			if (pastedIDs)
			{
				spanData.GraphMembers.Append(*pastedIDs);
			}
			else
			{   // graph ID is unchanged?
				spanData.GraphMembers.Add(spanMember);
			}
		}
		stateData.CustomData.SaveStructData(spanData, UE_BUILD_DEVELOPMENT);

		break;
	}

	case EObjectType::OTMetaPlaneSpan:
	{
		FMOIMetaPlaneSpanData spanData;
		Moi->GetStateData().CustomData.LoadStructData(spanData);
		spanData.GraphMembers.Empty();
		for (int32 spanMember : Cast<AMOIMetaPlaneSpan>(Moi)->InstanceData.GraphMembers)
		{
			const auto* pastedIDs = CopiedToPastedObjIDs.Find(spanMember);
			if (pastedIDs)
			{
				spanData.GraphMembers.Append(*pastedIDs);
			}
			else
			{   // graph ID is unchanged?
				spanData.GraphMembers.Add(spanMember);
			}
		}
		stateData.CustomData.SaveStructData(spanData, UE_BUILD_DEVELOPMENT);

		break;
	}

	default:
		return;;
	}

	newSpanDelta->AddMutationState(Moi, Moi->GetStateData(), stateData);
	OutDeltas.Add(MoveTemp(newSpanDelta));
}
