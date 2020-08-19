#include "ModumateCore/ModumateObjectDeltaStatics.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3DTypes.h"

bool FModumateObjectDeltaStatics::PreviewMovement(const TMap<int32, TArray<FVector>>& ObjectMovements, FModumateDocument *doc, UWorld *World)
{
	doc->ClearPreviewDeltas(World);

	TMap<int32, FVector> vertex3DMovements;

	auto& graph = doc->GetVolumeGraph();

	for (auto& kvp : ObjectMovements)
	{
		auto moi = doc->GetObjectById(kvp.Key);
		if (!ensureAlways(moi != nullptr))
		{
			continue;
		}

		EObjectType objectType = moi->GetObjectType();
		Modumate::EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);

		if (graph3DObjType != Modumate::EGraph3DObjectType::None)
		{
			TArray<int32> vertexIDs;
			if (moi->GetObjectType() == EObjectType::OTMetaVertex)
			{
				auto vertex = graph.FindVertex(moi->ID);
				if (vertex == nullptr)
				{
					return false;
				}
				vertexIDs = { vertex->ID };
			}
			else if (moi->GetObjectType() == EObjectType::OTMetaEdge)
			{
				auto edge = graph.FindEdge(moi->ID);
				if (edge == nullptr)
				{
					return false;
				}
				vertexIDs = { edge->StartVertexID, edge->EndVertexID };
			}
			else if (moi->GetObjectType() == EObjectType::OTMetaPlane)
			{
				auto face = graph.FindFace(moi->ID);
				if (face == nullptr)
				{
					return false;
				}
				vertexIDs = face->VertexIDs;
			}

			if (vertexIDs.Num() != kvp.Value.Num())
			{
				return false;
			}

			for (int32 vertexIdx = 0; vertexIdx < vertexIDs.Num(); vertexIdx++)
			{
				int32 vertexID = vertexIDs[vertexIdx];
				FVector position = kvp.Value[vertexIdx];
				if (!vertex3DMovements.Contains(vertexID))
				{
					vertex3DMovements.Add(vertexID, position);
				}
			}
		}
		else
		{	// TODO: non-3D Graph objects
			return false;
		}
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

		TArray<TSharedPtr<Modumate::FDelta>> deltas;
		if (!doc->GetPreviewVertexMovementDeltas(vertexMoveIDs, vertexMovePositions, deltas))
		{
			return false;
		}
		doc->ApplyPreviewDeltas(deltas, World);
	}
	else
	{
		return false;
	}

	return true;
}
