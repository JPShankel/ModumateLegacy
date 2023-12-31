// Copyright 2020 Modumate, Inc. All Rights Reserved.

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
#include "Objects/DesignOption.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/MetaPlaneSpan.h"
#include "Objects/ModumateSymbolDeltaStatics.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"

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
				TSet<AModumateObjectInstance*> groupMembers;
				doc->GetVolumeGraph(id)->GetVertices().GenerateKeyArray(allVertexIDs);
				OutTransformableIDs.Append(allVertexIDs);
				UModumateObjectStatics::GetObjectsInGroups(doc, { id }, groupMembers);
				for (const auto* object : groupMembers)
				{
					if (object->GetObjectType() == EObjectType::OTFurniture)
					{
						OutTransformableIDs.Add(object->ID);
					}
				}
				OutTransformableIDs.Add(moi->ID);  // Add group itself.
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

	// Third pass - Groups (for paste tool, vector-copy handled in the move tool).
	TSet<int32> groupIDs;
	for (const auto& objRec : InRecord->ObjectData)
	{
		if (objRec.ObjectType == EObjectType::OTMetaGraph && Doc->GetVolumeGraph(objRec.ID))
		{
			groupIDs.Add(objRec.ID);
		}
	}
	if (groupIDs.Num() > 0)
	{
		TArray<TPair<FSelectedObjectToolMixin::CopyDeltaType, FDeltaPtr>> groupCopyDeltas;
		DuplicateGroups(Doc, groupIDs, nextID, groupCopyDeltas);
		// Apply offset:
		GetDeltasForGroupCopies(Doc, offset, groupCopyDeltas, OutDeltas);
	}

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

void FModumateObjectDeltaStatics::DuplicateGroups(const UModumateDocument* Doc, const TSet<int32>& GroupIDs, int32& NextID,
	TArray<TPair<FSelectedObjectToolMixin::CopyDeltaType, FDeltaPtr>>& OutDeltas)
{
	TMap<int32, int32> oldIDToNewID;
	oldIDToNewID.Add(MOD_ID_NONE, MOD_ID_NONE);

	// Create new group IDs first to be able to preserve hierarchy.
	for (int32 id : GroupIDs)
	{
		oldIDToNewID.Add(id, NextID++);
	}

	// Create map of existing mapping from group to its design option, if any.
	TMap<int32, TArray<int32>> groupToOptionMap;

	TArray<const AMOIDesignOption*> designOptionMois;
	Doc->GetObjectsOfTypeCasted(EObjectType::OTDesignOption, designOptionMois);
	for (const auto* moi: designOptionMois)
	{
		for (int32 g : moi->InstanceData.groups)
		{
			groupToOptionMap.FindOrAdd(g).Add(moi->ID);
		}
	}

	TMap<int32, TArray<int32>> optionToNewGroupsMap;

	for (int32 groupID: GroupIDs)
	{
		const FGraph3D* graph = Doc->GetVolumeGraph(groupID);
		if (!ensure(graph))
		{
			return;
		}

		const AMOIMetaGraph* graphObject = Cast<AMOIMetaGraph>(Doc->GetObjectById(groupID));
		if (!ensure(graphObject))
		{
			return;
		}

		int32 parentID = graphObject->GetParentID();
		if (parentID == MOD_ID_NONE)
		{   // Can't duplicate root group
			return;
		}

		int32 newGroupID = oldIDToNewID[groupID];
		if (oldIDToNewID.Contains(parentID))
		{
			parentID = oldIDToNewID[parentID];
		}

		FMOIStateData stateData(newGroupID, EObjectType::OTMetaGraph, parentID);
		stateData.AssemblyGUID = graphObject->GetStateData().AssemblyGUID;
		FMOIMetaGraphData groupInstanceData(graphObject->InstanceData);
		stateData.CustomData.SaveStructData(groupInstanceData);

		auto delta = MakeShared<FMOIDelta>();
		delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
		OutDeltas.Emplace(FSelectedObjectToolMixin::kGroup, delta);

		auto addGraph = MakeShared<FGraph3DDelta>();
		addGraph->DeltaType = EGraph3DDeltaType::Add;
		addGraph->GraphID = newGroupID;
		OutDeltas.Emplace(FSelectedObjectToolMixin::kOther, addGraph);

		TArray<const AModumateObjectInstance*> objects;
		for (const auto& moiKvp : graph->GetAllObjects())
		{
			int32 moiID = moiKvp.Key;
			const AModumateObjectInstance* moi = Doc->GetObjectById(moiID);
			if (moi)
			{
				objects.Add(moi);
				UModumateObjectStatics::GetAllDescendents(moi, objects);
			}
		}

		// Pre-allocate all new IDs other than surface-graph elements.
		for (const auto* obj : objects)
		{
			if (UModumateTypeStatics::Graph2DObjectTypeFromObjectType(obj->GetObjectType()) == EGraphObjectType::None)
			{
				oldIDToNewID.Add(obj->GetStateData().ID, NextID++);
			}
		}

		auto newElementsDelta = MakeShared<FGraph3DDelta>();
		newElementsDelta->GraphID = newGroupID;
		for (const auto& kvp : graph->GetVertices())
		{
			newElementsDelta->VertexAdditions.Add(oldIDToNewID[kvp.Key], kvp.Value.Position);
		}
		for (const auto& kvp : graph->GetEdges())
		{
			FGraph3DObjDelta newEdge(FGraphVertexPair(oldIDToNewID[kvp.Value.StartVertexID], oldIDToNewID[kvp.Value.EndVertexID]), TArray<int32>());
			newElementsDelta->EdgeAdditions.Add(oldIDToNewID[kvp.Key], newEdge);
		}
		for (const auto& kvp : graph->GetFaces())
		{
			TArray<int32> newVertices;
			for (int32 v : kvp.Value.VertexIDs)
			{
				newVertices.Add(oldIDToNewID[v]);
			}
			FGraph3DObjDelta newFace(newVertices, TArray<int32>(), kvp.Value.ContainingFaceID, kvp.Value.ContainedFaceIDs);
			newElementsDelta->FaceAdditions.Add(oldIDToNewID[kvp.Key], newFace);
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

		OutDeltas.Emplace(FSelectedObjectToolMixin::kVertexPosition, newElementsDelta);

		auto moiDelta = MakeShared<FMOIDelta>();
		auto ffeDelta = MakeShared<FMOIDelta>();

		TArray<FDeltaPtr> objectDeltas;

		// Duplicate surface-graph MOIs before actual surface graphs:
		auto surfaceGraphMoiDelta = MakeShared<FMOIDelta>();
		TArray<int32> surfaceGraphIDs;  // Existing IDs
		for (const auto* object : objects)
		{
			const auto& moiDef = object->GetStateData();
			if (moiDef.ObjectType == EObjectType::OTSurfaceGraph)
			{
				FMOIStateData newState(moiDef);
				surfaceGraphIDs.Add(newState.ID);
				newState.ID = oldIDToNewID[newState.ID];
				newState.ParentID = oldIDToNewID[newState.ParentID];
				surfaceGraphMoiDelta->AddCreateDestroyState(newState, EMOIDeltaType::Create);
			}
		}
		if (surfaceGraphMoiDelta->IsValid())
		{
			OutDeltas.Emplace(FSelectedObjectToolMixin::kOther, surfaceGraphMoiDelta);

			for (int32 oldGraphId: surfaceGraphIDs)
			{
				int32 newGraphID = oldIDToNewID[oldGraphId];
				auto makeGraph2dDelta = MakeShared<FGraph2DDelta>(newGraphID, EGraph2DDeltaType::Add);
				auto graph2dDelta = MakeShared<FGraph2DDelta>(newGraphID);
				const auto graph2d = Doc->FindSurfaceGraph(oldGraphId);

				for (const auto& kvp: graph2d->GetVertices())
				{
					oldIDToNewID.Add(kvp.Key, NextID);
					graph2dDelta->AddNewVertex(kvp.Value.Position, NextID);
				}

				for (const auto& kvp: graph2d->GetEdges())
				{
					oldIDToNewID.Add(kvp.Key, NextID);
					graph2dDelta->AddNewEdge(FGraphVertexPair(oldIDToNewID[kvp.Value.StartVertexID], oldIDToNewID[kvp.Value.EndVertexID]), NextID);
				}

				for (const auto& kvp: graph2d->GetPolygons())
				{
					TArray<int32> newVertices;
					Algo::ForEach(kvp.Value.VertexIDs, [&](int32 v)
						{ newVertices.Add(oldIDToNewID[v]); });
					oldIDToNewID.Add(kvp.Key, NextID);
					graph2dDelta->AddNewPolygon(newVertices, NextID);
				}

				OutDeltas.Emplace(FSelectedObjectToolMixin::kOther, makeGraph2dDelta);
				OutDeltas.Emplace(FSelectedObjectToolMixin::kOther, graph2dDelta);
			}
		}


		for (const auto* object : objects)
		{
			EObjectType objectType = object->GetObjectType();
			if (objectType != EObjectType::OTSurfaceGraph
				&& UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType) == EGraph3DObjectType::None
				&& UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objectType) == EGraphObjectType::None)
			{
				FMOIStateData newState(object->GetStateData());
				newState.ID = oldIDToNewID[newState.ID];
				newState.ParentID = oldIDToNewID[newState.ParentID];

				// Duplicate span objects mapping graph IDs:
				switch (objectType)
				{
				case EObjectType::OTMetaEdgeSpan:
				{
					FMOIMetaEdgeSpanData instanceData(Cast<const AMOIMetaEdgeSpan>(object)->InstanceData);
					Algo::ForEach(instanceData.GraphMembers, [&oldIDToNewID](int32& S)
						{ S = oldIDToNewID[S]; });
					newState.CustomData.SaveStructData(instanceData, UE_EDITOR);
					break;
				}

				case EObjectType::OTMetaPlaneSpan:
				{
					FMOIMetaPlaneSpanData instanceData(Cast<const AMOIMetaPlaneSpan>(object)->InstanceData);
					Algo::ForEach(instanceData.GraphMembers, [&oldIDToNewID](int32& S)
						{ S = oldIDToNewID[S]; });
					newState.CustomData.SaveStructData(instanceData, UE_EDITOR);
					break;
				}

				case EObjectType::OTFurniture:
				{
					ffeDelta->AddCreateDestroyState(newState, EMOIDeltaType::Create);
					continue;
				}

				default:
					break;
				}

				moiDelta->AddCreateDestroyState(newState, EMOIDeltaType::Create);
			}
		}

		TArray<int32>* optionIDs = groupToOptionMap.Find(groupID);
		if (optionIDs)
		{
			for (int32 optionID: *optionIDs)
			{
				optionToNewGroupsMap.FindOrAdd(optionID).Add(newGroupID);
			}
		}

		if (moiDelta->IsValid())
		{
			OutDeltas.Emplace(FSelectedObjectToolMixin::kOther, moiDelta);
		}
		if (ffeDelta->IsValid())
		{
			OutDeltas.Emplace(FSelectedObjectToolMixin::kFfe, ffeDelta);
		}
	}

	FBIMSymbolCollectionProxy symbolsCollection(&Doc->GetPresetCollection());

	// Update EquivalentIDs in any Symbol Presets:
	for (int32 groupID : GroupIDs)
	{
		const AMOIMetaGraph* graphObject = Cast<AMOIMetaGraph>(Doc->GetObjectById(groupID));
		const FGuid& symbolID = graphObject->GetStateData().AssemblyGUID;

		if (symbolID.IsValid())
		{
			FBIMSymbolPresetData* symbolPresetData = symbolsCollection.PresetDataFromGUID(symbolID);
			if (ensure(symbolPresetData))
			{
				for (auto& IdMap : symbolPresetData->EquivalentIDs)
				{
					TSet<int32> newIDs;
					for (int32 id : IdMap.Value.IDSet)
					{
						const auto* newID = oldIDToNewID.Find(id);
						if (newID)
						{
							newIDs.Add(*newID);
						}
					}
					IdMap.Value.IDSet.Append(MoveTemp(newIDs));
				}

			}
		}
	}

	TArray<FDeltaPtr> symbolsDeltas;
	symbolsCollection.GetPresetDeltas(symbolsDeltas);
	for (const auto& delta: symbolsDeltas)
	{
		OutDeltas.Emplace(FSelectedObjectToolMixin::kPreset, delta);
	}

	if (optionToNewGroupsMap.Num() > 0)
	{
		auto optionsMoiDelta = MakeShared<FMOIDelta>();
		for (const auto& optionKvp : optionToNewGroupsMap)
		{
			const AMOIDesignOption* optionMoi = Cast<AMOIDesignOption>(Doc->GetObjectById(optionKvp.Key));
			if (ensure(optionMoi))
			{
				FMOIDesignOptionData instanceData = optionMoi->InstanceData;
				instanceData.groups.Append(optionKvp.Value);
				optionsMoiDelta->AddMutationState(optionMoi).CustomData.SaveStructData(instanceData);
			}
		}

		OutDeltas.Emplace(FSelectedObjectToolMixin::kOther, optionsMoiDelta);
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

void FModumateObjectDeltaStatics::GetDeltasForGroupTransforms(UModumateDocument* Doc, const TMap<int32,FTransform>& OriginalGroupVertexTranslations, const FTransform transform, TArray<FDeltaPtr>& OutDeltas)
{
	TMap<int32, FGraph3DDelta> graphDeltas;
	auto moiDelta = MakeShared<FMOIDelta>();

	for (const auto& kvp : OriginalGroupVertexTranslations)
	{
		int id = kvp.Key;
		const FVector position = kvp.Value.GetLocation();
		int32 graphId = Doc->FindGraph3DByObjID(id);
		if (graphId != MOD_ID_NONE)
		{
			FGraph3D* graph = Doc->GetVolumeGraph(graphId);
			const FGraph3DVertex* vertex = graph ? graph->GetVertices().Find(id) : nullptr;
			if (ensure(vertex))
			{
				FGraph3DDelta* graphDelta = graphDeltas.Find(graphId);
				if (!graphDelta)
				{
					graphDelta = &graphDeltas.Add(graphId, FGraph3DDelta(graphId));
				}
				graphDelta->VertexMovements.Add(id, FModumateVectorPair(position, transform.TransformPositionNoScale(position)));
			}
		}
		else
		{
			const AModumateObjectInstance* moi = Doc->GetObjectById(id);
			switch (moi->GetObjectType())
			{
			case EObjectType::OTMetaGraph:
			{
				const AMOIMetaGraph* groupMoi = Cast<AMOIMetaGraph>(moi);
				FMOIStateData groupData(groupMoi->GetStateData());
				FMOIStateData oldGroupData(groupData);
				FMOIMetaGraphData groupCustomData;
				groupData.CustomData.LoadStructData(groupCustomData);
				groupCustomData.Location = transform.TransformPositionNoScale(position);
				groupCustomData.Rotation = transform.GetRotation() * kvp.Value.GetRotation();
				groupData.CustomData.SaveStructData(groupCustomData, UE_EDITOR);

				// Re-create original transform.
				groupCustomData.Location = position;
				groupCustomData.Rotation = kvp.Value.GetRotation();
				oldGroupData.CustomData.SaveStructData(groupCustomData, UE_EDITOR);

				moiDelta->AddMutationState(groupMoi, oldGroupData, groupData);
				break;
			}

			case EObjectType::OTFurniture:
			{
				FMOIStateData oldState = moi->GetStateData();
				FMOIStateData newState(oldState);
				moi->GetTransformedLocationState(kvp.Value, oldState);
				moi->GetTransformedLocationState(kvp.Value * transform, newState);
				moiDelta->AddMutationState(moi, oldState, newState);
				break;
			}

			default:
				check(false);
				break;
			}
		}
	}

	for (auto& graphKvp : graphDeltas)
	{
		OutDeltas.Add(MakeShared<FGraph3DDelta>(MoveTemp(graphKvp.Value) ));
	}
	OutDeltas.Add(moiDelta);
}

void FModumateObjectDeltaStatics::GetDeltasForGraphDelete(const UModumateDocument* Doc, int32 GraphID, TArray<FDeltaPtr>& OutDeltas,
	bool bDeleteGraphMoi /*= true*/, FBIMSymbolCollectionProxy* SymbolCollection /*= nullptr*/)
{
	auto* graph = Doc->GetVolumeGraph(GraphID);
	TMap<int32, FGraph2DDelta> surfaceGraphDeltas;
	TSet<int32> deletedIDs;

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
				deletedIDs.Add(metaObject->ID);
				for (auto* moi : mois)
				{
					deletedIDs.Add(moi->ID);

					EGraphObjectType graph2dType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType());
					if (graph2dType != EGraphObjectType::None)
					{
						auto graph2d = Doc->FindSurfaceGraph(moi->GetParentID());
						if (ensure(graph2d.IsValid()))
						{
							auto& graph2dDelta = surfaceGraphDeltas.FindOrAdd(graph2d->GetID());
							switch (graph2dType)
							{
							case EGraphObjectType::Vertex:
								graph2dDelta.VertexDeletions.Add(moi->ID, graph2d->FindVertex(moi->ID)->Position);
								break;

							case EGraphObjectType::Edge:
							{
								const auto* edge = graph2d->FindEdge(moi->ID);
								graph2dDelta.EdgeDeletions.Add(moi->ID, FGraph2DObjDelta({ edge->StartVertexID, edge->EndVertexID }));
								break;
							}

							case EGraphObjectType::Polygon:
							{
								const auto* face = graph2d->FindPolygon(moi->ID);
								graph2dDelta.PolygonDeletions.Add(moi->ID, FGraph2DObjDelta(face->VertexIDs));
								break;
							}
							}
						}
					}
					else if (!UModumateTypeStatics::IsSpanObject(moi))
					{   // Spans are deleted via regular side-effect deltas.
						moisDelta->AddCreateDestroyState(moi->GetStateData(), EMOIDeltaType::Destroy);
					}
				}
			}
		}

		for (auto& graph2dDelta: surfaceGraphDeltas)
		{
			graph2dDelta.Value.ID = graph2dDelta.Key;
			OutDeltas.Add(MakeShared<FGraph2DDelta>(graph2dDelta.Value));
			OutDeltas.Add(MakeShared<FGraph2DDelta>(graph2dDelta.Key, EGraph2DDeltaType::Remove));
		}

		if (moisDelta->IsValid())
		{
			OutDeltas.Add(moisDelta);
		}

		if (!graphDelta->IsEmpty())
		{
			OutDeltas.Add(graphDelta);

			if (bDeleteGraphMoi)
			{
				TSharedPtr<FGraph3DDelta> deleteGraph = MakeShared<FGraph3DDelta>(GraphID);
				deleteGraph->DeltaType = EGraph3DDeltaType::Remove;
				OutDeltas.Add(deleteGraph);
				TSharedPtr<FMOIDelta> deleteMetaGraph = MakeShared<FMOIDelta>();
				deleteMetaGraph->AddCreateDestroyState(Doc->GetObjectById(GraphID)->GetStateData(), EMOIDeltaType::Destroy);
				OutDeltas.Add(deleteMetaGraph);
			}
		}

		if (SymbolCollection)
		{
			const AModumateObjectInstance* groupMoi = Doc->GetObjectById(GraphID);
			// If group is symbol then remove deleted items from Equivalence map.
			if (groupMoi && groupMoi->GetStateData().AssemblyGUID.IsValid())
			{
				auto childGroups = groupMoi->GetAllDescendents();
				Algo::ForEach(childGroups, [&deletedIDs](const AModumateObjectInstance* moi) { deletedIDs.Add(moi->ID); });
				auto* symbolData = SymbolCollection->PresetDataFromGUID(groupMoi->GetStateData().AssemblyGUID);
				for (auto& set : symbolData->EquivalentIDs)
				{
					set.Value.IDSet = set.Value.IDSet.Difference(deletedIDs);
				}
			}
		}
	}
}

void FModumateObjectDeltaStatics::GetDeltasForGraphDeleteRecursive(const UModumateDocument* Doc, int32 GraphID, FBIMSymbolCollectionProxy* SymbolCollection,
	TArray<FDeltaPtr>& OutDeltas, bool bDeleteGraphMoi /*= true*/)
{
	auto* groupObject = Doc->GetObjectById(GraphID);
	if (groupObject)
	{
		TArray<const AModumateObjectInstance*> descendants = groupObject->GetAllDescendents();
		GetDeltasForGraphDelete(Doc, GraphID, OutDeltas, bDeleteGraphMoi, SymbolCollection);
		for (const auto* subObject: descendants)
		{
			GetDeltasForGraphDelete(Doc, subObject->ID, OutDeltas, true, SymbolCollection);
		}
	}
}

template<class T>
void GetDeltasForSpanMerge(const UModumateDocument* Doc, const TArray<int32>& SpanIDs, TArray<FDeltaPtr>& OutDeltas)
{
	TArray<const T*> allSpans;
	Algo::TransformIf(SpanIDs, allSpans,
		[Doc](int32 MOI)
		{
			return Cast<T>(Doc->GetObjectById(MOI));
		},
		[Doc](int32 MOI)
		{
			return Cast<T>(Doc->GetObjectById(MOI));
		});

	if (allSpans.Num() < 2)
	{
		return;
	}

	typename T::FInstanceData mergeData = allSpans[0]->InstanceData;
	TSharedPtr<FMOIDelta> moisDelta = MakeShared<FMOIDelta>();
	for (int32 i = 1; i < allSpans.Num(); ++i)
	{
		mergeData.GraphMembers.Append(allSpans[i]->InstanceData.GraphMembers);
		for (int32 childID : allSpans[i]->GetChildIDs())
		{
			const AModumateObjectInstance* moi = Doc->GetObjectById(childID);
			if (moi != nullptr)
			{
				moisDelta->AddCreateDestroyState(moi->GetStateData(), EMOIDeltaType::Destroy);
			}
		}
		moisDelta->AddCreateDestroyState(allSpans[i]->GetStateData(), EMOIDeltaType::Destroy);
	}
	FMOIStateData mergeState = allSpans[0]->GetStateData();
	mergeState.CustomData.SaveStructData(mergeData);
	moisDelta->AddMutationState(allSpans[0], allSpans[0]->GetStateData(), mergeState);
	OutDeltas.Add(moisDelta);
}

void FModumateObjectDeltaStatics::GetDeltasForEdgeSpanMerge(const UModumateDocument* Doc, const TArray<int32>& SpanIDs, TArray<FDeltaPtr>& OutDeltas)
{
	GetDeltasForSpanMerge<AMOIMetaEdgeSpan>(Doc, SpanIDs, OutDeltas);
}

void FModumateObjectDeltaStatics::GetDeltasForFaceSpanMerge(const UModumateDocument* Doc, const TArray<int32>& SpanIDs, TArray<FDeltaPtr>& OutDeltas)
{
	GetDeltasForSpanMerge<AMOIMetaPlaneSpan>(Doc, SpanIDs, OutDeltas);
}

template<class T>
void GetDeltasForSpanSplitT(const UModumateDocument* Doc, const T* SpanOb, int32& NextID, TArray<FDeltaPtr>& OutDeltas, TSet<int32>* OutNewMOIs = nullptr)
{
	typename T::FInstanceData originalData = SpanOb->InstanceData;
	if (originalData.GraphMembers.Num() < 2)
	{
		return;
	}

	TSharedPtr<FMOIDelta> moisDelta = MakeShared<FMOIDelta>();

	for (int32 i = 1; i < originalData.GraphMembers.Num(); ++i)
	{
		FMOIStateData stateData = SpanOb->GetStateData();
		stateData.ID = NextID++;

		typename T::FInstanceData instanceData;
		instanceData.GraphMembers.Add(originalData.GraphMembers[i]);
		stateData.CustomData.SaveStructData(instanceData);

		moisDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

		int32 parentID = stateData.ID;

		for (auto& childId : SpanOb->GetChildIDs())
		{
			const AModumateObjectInstance* childOb = Doc->GetObjectById(childId);
			if (childOb != nullptr)
			{
				stateData = childOb->GetStateData();
				stateData.ID = NextID++;
				stateData.ParentID = parentID;
				moisDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
				if (OutNewMOIs)
				{
					OutNewMOIs->Add(stateData.ID);
				}
			}
		}
	}

	originalData.GraphMembers.SetNum(1);
	FMOIStateData newState = SpanOb->GetStateData();
	newState.CustomData.SaveStructData(originalData);
	moisDelta->AddMutationState(SpanOb, SpanOb->GetStateData(), newState);

	OutDeltas.Add(moisDelta);
}

void FModumateObjectDeltaStatics::GetDeltasForSpanSplit(UModumateDocument* Doc, const TArray<int32>& SpanIDs, TArray<FDeltaPtr>& OutDeltas,
	TSet<int32>* OutNewMOIs /*= nullptr*/)
{
	if (SpanIDs.Num() == 0)
	{
		return;
	}
	int32 nextID = Doc->ReserveNextIDs(SpanIDs[0]);
	for (int32 spanID : SpanIDs)
	{
		const AModumateObjectInstance* spanOb = Doc->GetObjectById(spanID);
		if (spanOb->GetObjectType() == EObjectType::OTMetaPlaneSpan)
		{
			GetDeltasForSpanSplitT(Doc, Cast<AMOIMetaPlaneSpan>(spanOb), nextID, OutDeltas, OutNewMOIs);
		}
		else if (spanOb->GetObjectType() == EObjectType::OTMetaEdgeSpan)
		{
			GetDeltasForSpanSplitT(Doc, Cast<AMOIMetaEdgeSpan>(spanOb), nextID, OutDeltas, OutNewMOIs);
		}
	}
	Doc->SetNextID(nextID, SpanIDs[0]);
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
		return;
	}

	newSpanDelta->AddMutationState(Moi, Moi->GetStateData(), stateData);
	OutDeltas.Add(MoveTemp(newSpanDelta));
}

bool FModumateObjectDeltaStatics::GetEdgeSpanCreationDeltas(const TArray<int32>& InTargetEdgeIDs, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewSpanID, int32& OutNewObjID)
{
	if (InTargetEdgeIDs.Num() == 0)
	{
		return false;
	}

	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	OutDeltaPtrs.Add(delta);

	// Create new span that contain InTargetEdgeIDs
	FMOIStateData spanCreateState(InNextID++, EObjectType::OTMetaEdgeSpan);
	FMOIMetaEdgeSpanData spanData;
	spanData.GraphMembers = InTargetEdgeIDs;
	spanCreateState.CustomData.SaveStructData(spanData);
	delta->AddCreateDestroyState(spanCreateState, EMOIDeltaType::Create);

	// Create edge hosted object that will be child of the new preview span
	MOIStateData.ParentID = spanCreateState.ID;
	MOIStateData.ID = InNextID++;
	MOIStateData.AssemblyGUID = InAssemblyGUID;
	delta->AddCreateDestroyState(MOIStateData, EMOIDeltaType::Create);

	OutNewSpanID = spanCreateState.ID;
	OutNewObjID = MOIStateData.ID;

	return true;
}

bool FModumateObjectDeltaStatics::GetFaceSpanCreationDeltas(const TArray<int32>& InTargetFaceIDs, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewSpanID, int32& OutNewObjID)
{
	if (InTargetFaceIDs.Num() == 0)
	{
		return false;
	}

	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	OutDeltaPtrs.Add(delta);

	// Create new span that contain InTargetFaceIDs
	FMOIStateData spanCreateState(InNextID++, EObjectType::OTMetaPlaneSpan);
	FMOIMetaPlaneSpanData spanData;
	spanData.GraphMembers = InTargetFaceIDs;
	spanCreateState.CustomData.SaveStructData(spanData);
	delta->AddCreateDestroyState(spanCreateState, EMOIDeltaType::Create);

	// Create edge hosted object that will be child of the new preview span
	MOIStateData.ParentID = spanCreateState.ID;
	MOIStateData.ID = InNextID++;
	MOIStateData.AssemblyGUID = InAssemblyGUID;
	delta->AddCreateDestroyState(MOIStateData, EMOIDeltaType::Create);

	OutNewSpanID = spanCreateState.ID;
	OutNewObjID = MOIStateData.ID;

	return true;
}

bool FModumateObjectDeltaStatics::GetEdgeSpanEditAssemblyDeltas(const UModumateDocument* Doc, const int32& InTargetSpanID, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewObjID)
{
	const AMOIMetaEdgeSpan* spanMOI = Cast<AMOIMetaEdgeSpan>(Doc->GetObjectById(InTargetSpanID));
	if (!spanMOI)
	{
		return false;
	}

	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	OutDeltaPtrs.Add(delta);

	for (auto* childOb : spanMOI->GetChildObjects())
	{
		delta->AddCreateDestroyState(childOb->GetStateData(), EMOIDeltaType::Destroy);
	}

	MOIStateData.ParentID = spanMOI->ID;
	MOIStateData.ID = InNextID++;
	MOIStateData.AssemblyGUID = InAssemblyGUID;
	delta->AddCreateDestroyState(MOIStateData, EMOIDeltaType::Create);

	OutNewObjID = MOIStateData.ID;

	return true;
}

bool FModumateObjectDeltaStatics::GetFaceSpanEditAssemblyDeltas(const UModumateDocument* Doc, const int32& InTargetSpanID, int32& InNextID, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, int32& OutNewObjID)
{
	const AMOIMetaPlaneSpan* spanMOI = Cast<AMOIMetaPlaneSpan>(Doc->GetObjectById(InTargetSpanID));
	if (!spanMOI)
	{
		return false;
	}

	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	OutDeltaPtrs.Add(delta);

	for (auto* childOb : spanMOI->GetChildObjects())
	{
		delta->AddCreateDestroyState(childOb->GetStateData(), EMOIDeltaType::Destroy);
	}

	MOIStateData.ParentID = spanMOI->ID;
	MOIStateData.ID = InNextID++;
	MOIStateData.AssemblyGUID = InAssemblyGUID;
	delta->AddCreateDestroyState(MOIStateData, EMOIDeltaType::Create);

	OutNewObjID = MOIStateData.ID;

	return true;
}

bool FModumateObjectDeltaStatics::GetObjectCreationDeltasForEdgeSpans(const UModumateDocument* Doc, EToolCreateObjectMode CreationMode, const TArray<int32>& InTargetEdgeIDs, int32& InNextID, int32 InTargetSpanIndex, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, TArray<int32>& OutNewObjectIDs)
{
	// Add mode: Create new EdgeSpan
	// Apply mode: Find EdgeSpan from MetaEdge, edit its assembly, but if no span exist, same as Add mode
	for (int32 targetEdgeID : InTargetEdgeIDs)
	{
		bool bAddSpan = CreationMode == EToolCreateObjectMode::Add || CreationMode == EToolCreateObjectMode::Draw;
		TArray<int32> spans;
		const AModumateObjectInstance* targetEdgeMOI = Doc->GetObjectById(targetEdgeID);
		if (targetEdgeMOI && targetEdgeMOI->GetObjectType() == EObjectType::OTMetaEdge)
		{
			UModumateObjectStatics::GetSpansForEdgeObject(Doc, targetEdgeMOI, spans);
		}
		const AMOIMetaEdgeSpan* spanObj = spans.Num() > 0 ? Cast<AMOIMetaEdgeSpan>(Doc->GetObjectById(spans[InTargetSpanIndex])) : nullptr;
		if (!spanObj)
		{
			bAddSpan = true;
		}

		bool bSuccess = false;
		if (bAddSpan)
		{
			int32 newSpanID;
			int32 newEdgeHostedObjID;
			if (GetEdgeSpanCreationDeltas({ targetEdgeID }, InNextID, InAssemblyGUID, MOIStateData, OutDeltaPtrs, newSpanID, newEdgeHostedObjID))
			{
				OutNewObjectIDs = { newSpanID, newEdgeHostedObjID };
				bSuccess = true;
			}
		}
		else
		{
			int32 newEdgeHostedObjID;
			if (GetEdgeSpanEditAssemblyDeltas(Doc, spanObj->ID, InNextID, InAssemblyGUID, MOIStateData, OutDeltaPtrs, newEdgeHostedObjID))
			{
				OutNewObjectIDs = { newEdgeHostedObjID };
				bSuccess = true;
			}
		}
	}
	return true;
}

bool FModumateObjectDeltaStatics::GetObjectCreationDeltasForFaceSpans(const UModumateDocument* Doc, EToolCreateObjectMode CreationMode, const TArray<int32>& InTargetFaceIDs, int32& InNextID, int32 InTargetSpanIndex, const FGuid& InAssemblyGUID, FMOIStateData& MOIStateData, TArray<FDeltaPtr>& OutDeltaPtrs, TArray<int32>& OutNewObjectIDs)
{
	// Add mode: Create new PlaneSpan
	// Apply mode: Find PlaneSpan from MetaPlane, edit its assembly, but if no span exist, same as Add mode
	for (int32 targetFaceID : InTargetFaceIDs)
	{
		bool bAddSpan = CreationMode == EToolCreateObjectMode::Add || CreationMode == EToolCreateObjectMode::Draw;
		TArray<int32> spans;
		const AModumateObjectInstance* targetFaceMOI = Doc->GetObjectById(targetFaceID);
		if (targetFaceMOI && targetFaceMOI->GetObjectType() == EObjectType::OTMetaPlane)
		{
			UModumateObjectStatics::GetSpansForFaceObject(Doc, targetFaceMOI, spans);
		}
		const AMOIMetaPlaneSpan* spanObj = spans.Num() > 0 ? Cast<AMOIMetaPlaneSpan>(Doc->GetObjectById(spans[InTargetSpanIndex])) : nullptr;
		if (!spanObj)
		{
			bAddSpan = true;
		}

		bool bSuccess = false;
		if (bAddSpan)
		{
			int32 newSpanID;
			int32 newPlaneHostedObjID;
			if (GetFaceSpanCreationDeltas({ targetFaceID }, InNextID, InAssemblyGUID, MOIStateData, OutDeltaPtrs, newSpanID, newPlaneHostedObjID))
			{
				OutNewObjectIDs = { newSpanID, newPlaneHostedObjID };
				bSuccess = true;
			}
		}
		else
		{
			int32 newPlaneHostedObjID;
			if (GetFaceSpanEditAssemblyDeltas(Doc, spanObj->ID, InNextID, InAssemblyGUID, MOIStateData, OutDeltaPtrs, newPlaneHostedObjID))
			{
				OutNewObjectIDs = { newPlaneHostedObjID };
				bSuccess = true;
			}
		}
	}
	return true;
}

void FModumateObjectDeltaStatics::GetDeltasForGroupCopies(UModumateDocument* Doc, FVector Offset,
	const TArray<TPair<FSelectedObjectToolMixin::CopyDeltaType, FDeltaPtr>>& GroupCopyDeltas, TArray<FDeltaPtr>& OutDeltas,
	bool bPresetsAlso /*= true*/)
{
	for (const auto& deltaPair : GroupCopyDeltas)
	{
		FDeltaPtr delta = deltaPair.Value;

		switch (FSelectedObjectToolMixin::CopyDeltaType(deltaPair.Key))
		{
		case FSelectedObjectToolMixin::kVertexPosition:
		{
			FGraph3DDelta graphDelta(*static_cast<FGraph3DDelta*>(delta.Get()));
			for (auto& kvp : graphDelta.VertexAdditions)
			{
				kvp.Value += Offset;
			}
			OutDeltas.Add(MakeShared<FGraph3DDelta>(MoveTemp(graphDelta)));
			break;
		}

		case FSelectedObjectToolMixin::kGroup:
		{	// Update the reference transform on the group itself.
			const FMOIDelta* groupDelta = static_cast<FMOIDelta*>(delta.Get());
			FMOIMetaGraphData groupCustomData;

			check(groupDelta->States.Num() == 1 && groupDelta->States[0].DeltaType == EMOIDeltaType::Create);

			FMOIStateData groupData(groupDelta->States[0].NewState);

			groupData.CustomData.LoadStructData(groupCustomData);
			groupCustomData.Location += Offset;
			groupData.CustomData.SaveStructData(groupCustomData, UE_EDITOR);
			auto moiDelta = MakeShared<FMOIDelta>();
			moiDelta->AddCreateDestroyState(groupData, EMOIDeltaType::Create);
			OutDeltas.Add(moiDelta);
			break;
		}

		case FSelectedObjectToolMixin::kPreset:
		{
			if (bPresetsAlso)
			{
				OutDeltas.Add(delta);
			}
			break;
		}

		case FSelectedObjectToolMixin::kOther:
		{
			OutDeltas.Add(delta);
			break;
		}

		case FSelectedObjectToolMixin::kFfe:
		{
			const FMOIDelta* groupDelta = static_cast<FMOIDelta*>(delta.Get());
			auto ffeDelta = MakeShared<FMOIDelta>();
			for (auto& singleDelta: groupDelta->States)
			{
				FMOIStateData newFfeState(singleDelta.NewState);
				FMOIFFEData ffeData;
				if (ensure(newFfeState.CustomData.LoadStructData(ffeData)) )
				{
					ffeData.Location += Offset;
					newFfeState.CustomData.SaveStructData(ffeData);
					ffeDelta->AddCreateDestroyState(newFfeState, EMOIDeltaType::Create);
				}
			}
			OutDeltas.Add(ffeDelta);
			break;
		}

		}
	}
}

template<class T>
void GetDeltasForSpanAddRemove(const UModumateDocument* Doc, const T* SpanOb, const TArray<int32>& AddMembers, const TArray<int32>& RemoveMembers, TArray<FDeltaPtr>& OutDeltas)
{
	if (!SpanOb)
	{
		return;
	}

	typename T::FInstanceData newData = SpanOb->InstanceData;
	for (int32 addMem : AddMembers)
	{
		newData.GraphMembers.AddUnique(addMem);
	}
	for (int32 remMem : RemoveMembers)
	{
		newData.GraphMembers.Remove(remMem);
	}

	FMOIStateData newStateData = SpanOb->GetStateData();
	newStateData.CustomData.SaveStructData(newData);

	auto moiDelta = MakeShared<FMOIDelta>();
	moiDelta->AddMutationState(SpanOb, SpanOb->GetStateData(), newStateData);
	OutDeltas.Add(moiDelta);
}

void FModumateObjectDeltaStatics::GetDeltasForFaceSpanAddRemove(const UModumateDocument* Doc, int32 SpanID, const TArray<int32>& AddFaces, const TArray<int32>& RemoveFaces, TArray<FDeltaPtr>& OutDeltas)
{
	GetDeltasForSpanAddRemove(Doc, Cast<AMOIMetaPlaneSpan>(Doc->GetObjectById(SpanID)), AddFaces, RemoveFaces, OutDeltas);
}

void FModumateObjectDeltaStatics::GetDeltasForEdgeSpanAddRemove(const UModumateDocument* Doc, int32 SpanID, const TArray<int32>& AddEdges, const TArray<int32>& RemoveEdges, TArray<FDeltaPtr>& OutDeltas)
{
	GetDeltasForSpanAddRemove(Doc, Cast<AMOIMetaEdgeSpan>(Doc->GetObjectById(SpanID)), AddEdges, RemoveEdges, OutDeltas);
}
