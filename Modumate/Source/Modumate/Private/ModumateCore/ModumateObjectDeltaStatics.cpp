#include "ModumateCore/ModumateObjectDeltaStatics.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2DDelta.h"
#include "Graph/Graph3DTypes.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/CutPlane.h"
#include "Objects/FFE.h"

void FModumateObjectDeltaStatics::GetTransformableIDs(const TArray<int32>& InObjectIDs, UModumateDocument *doc, TSet<int32>& OutTransformableIDs)
{
	for (int32 id : InObjectIDs)
	{
		auto moi = doc->GetObjectById(id);
		if (!ensureAlways(moi != nullptr))
		{
			continue;
		}
		Modumate::EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType());

		if (auto graphObject = doc->GetVolumeGraph().FindObject(id))
		{
			TArray<int32> vertexIDs;
			graphObject->GetVertexIDs(vertexIDs);
			OutTransformableIDs.Append(vertexIDs);
		}
		else if (auto parentGraphObject = doc->GetVolumeGraph().FindObject(moi->GetParentID()))
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

	auto& graph = doc->GetVolumeGraph();

	for (auto& kvp : ObjectMovements)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (!ensureAlways(moi != nullptr))
		{
			continue;
		}
		Modumate::EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType());
		Modumate::EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType());

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
		else if (graph2DObjType != Modumate::EGraphObjectType::None)
		{
			int32 targetParentID = moi->GetParentID();
			auto surfaceObj = doc->GetObjectById(targetParentID);
			auto surfaceGraph = doc->FindSurfaceGraph(targetParentID);
			auto surfaceParent = surfaceObj ? surfaceObj->GetParentObject() : nullptr;
			if (!ensure(surfaceObj && surfaceGraph && surfaceParent))
			{
				continue;
			}

			int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceObj);

			TArray<FVector> facePoints;
			FVector faceNormal, faceAxisX, faceAxisY;
			if (!ensure(UModumateObjectStatics::GetGeometryFromFaceIndex(surfaceParent, surfaceGraphFaceIndex, facePoints, faceNormal, faceAxisX, faceAxisY)))
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

			for (auto& delta : surfaceGraphDeltas)
			{
				deltas.Add(MakeShareable(new FGraph2DDelta{ delta }));
			}
		}
	}
	else
	{
		for (auto& kvp : ObjectMovements)
		{
			AModumateObjectInstance* moi = doc->GetObjectById(kvp.Key);
			FMOIDelta delta;
			auto& currentData = delta.AddMutationState(moi);
			if (moi->GetTransformedLocationState(kvp.Value, currentData))
			{
				deltas.Add(MakeShared<FMOIDelta>(delta));
			}
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
