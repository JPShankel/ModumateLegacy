#include "Graph3DDelta.h"
#include "ModumateDocument.h"

namespace Modumate
{
	FGraph3DObjDelta::FGraph3DObjDelta(FVertexPair vertexPair)
	{
		Vertices = { vertexPair.Key, vertexPair.Value };
	}

	FGraph3DObjDelta::FGraph3DObjDelta(FVertexPair vertexPair, TArray<int32> parents)
	{
		Vertices = { vertexPair.Key, vertexPair.Value };
		ParentFaceIDs = parents;
	}

	void FGraph3DDelta::Reset()
	{
		VertexMovements.Reset();
		VertexAdditions.Reset();
		VertexDeletions.Reset();

		EdgeAdditions.Reset();
		EdgeDeletions.Reset();

		FaceAdditions.Reset();
		FaceDeletions.Reset();

		FaceVertexAdditions.Reset();
		FaceVertexRemovals.Reset();

		ParentIDUpdates.Reset();
	}

	bool FGraph3DDelta::IsEmpty()
	{
		if (VertexMovements.Num() > 0) return false;
		if (VertexAdditions.Num() > 0) return false;
		if (VertexDeletions.Num() > 0) return false;
		if (EdgeAdditions.Num() > 0) return false;
		if (EdgeDeletions.Num() > 0) return false;
		if (FaceAdditions.Num() > 0) return false;
		if (FaceDeletions.Num() > 0) return false;
		if (FaceVertexAdditions.Num() > 0) return false;
		if (FaceVertexRemovals.Num() > 0) return false;
		if (ParentIDUpdates.Num() > 0) return false;

		return true;
	}

	int32 FGraph3DDelta::FindAddedVertex(FVector position)
	{
		for (auto &kvp : VertexAdditions)
		{
			FVector otherVertex = kvp.Value;
			if (position.Equals(otherVertex, DEFAULT_GRAPH3D_EPSILON))
			{
				return kvp.Key;
			}
		}

		return MOD_ID_NONE;
	}

	int32 FGraph3DDelta::FindAddedEdge(FVertexPair edge)
	{
		FVertexPair flippedEdge = FVertexPair(edge.Value, edge.Key);
		for (auto &kvp : EdgeAdditions)
		{
			FVertexPair otherPair = FVertexPair(kvp.Value.Vertices[0], kvp.Value.Vertices[1]);
			if (otherPair == edge)
			{
				return kvp.Key;
			}
			else if (otherPair == flippedEdge)
			{
				return -kvp.Key;
			}
		}

		return MOD_ID_NONE;
	}

	void FGraph3DDelta::AggregateAddedObjects(TArray<int32> OutAddedFaceIDs, TArray<int32> OutAddedVertexIDs, TArray<int32> OutAddedEdgeIDs)
	{
		for (auto &kvp : FaceAdditions)
		{
		//	if (kvp.Value.ParentFaceIDs.Num() == 1 && GetObjectById(kvp.Value.ParentFaceIDs[0]) == nullptr)
			{
				OutAddedFaceIDs.Add(kvp.Key);
			}
		}
		for (auto &kvp : VertexAdditions)
		{
			OutAddedVertexIDs.Add(kvp.Key);
		}
		for (auto &kvp : EdgeAdditions)
		{
			OutAddedEdgeIDs.Add(kvp.Key);
		}

		for (auto &kvp : FaceDeletions)
		{
			OutAddedFaceIDs.Remove(kvp.Key);
		}
		for (auto &kvp : VertexDeletions)
		{
			OutAddedVertexIDs.Remove(kvp.Key);
		}
		for (auto &kvp : EdgeDeletions)
		{
			OutAddedEdgeIDs.Remove(kvp.Key);
		}
	}

	TSharedPtr<FDelta> FGraph3DDelta::MakeInverse() const
	{
		TSharedPtr<FGraph3DDelta> inverse = MakeShareable(new FGraph3DDelta());

		for (const auto &kvp : VertexMovements)
		{
			int32 vertexID = kvp.Key;
			const TPair<FVector, FVector> &vertexMovement = kvp.Value;

			inverse->VertexMovements.Add(vertexID, TPair<FVector, FVector>(vertexMovement.Value, vertexMovement.Key));
		}

		inverse->VertexAdditions = VertexDeletions;
		inverse->VertexDeletions = VertexAdditions;

		inverse->EdgeAdditions = EdgeDeletions;
		inverse->EdgeDeletions = EdgeAdditions;

		inverse->FaceVertexAdditions = FaceVertexRemovals;
		inverse->FaceVertexRemovals = FaceVertexAdditions;

		inverse->FaceAdditions = FaceDeletions;
		inverse->FaceDeletions = FaceAdditions;

		for (const auto &kvp : ParentIDUpdates)
		{
			int32 parentID = kvp.Key;
			const FGraph3DHostedObjectDelta &idUpdate = kvp.Value;

			inverse->ParentIDUpdates.Add(parentID, FGraph3DHostedObjectDelta(idUpdate.PreviousHostedObjID, idUpdate.NextParentID, idUpdate.PreviousParentID));
		}

		return inverse;
	}

	bool FGraph3DDelta::ApplyTo(FModumateDocument *doc, UWorld *world) const
	{
		doc->ApplyGraph3DDelta(*this, world);
		return true;
	}
}
