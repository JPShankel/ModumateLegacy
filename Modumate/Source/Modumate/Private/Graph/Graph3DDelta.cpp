#include "Graph/Graph3DDelta.h"
#include "DocumentManagement/ModumateDocument.h"

namespace Modumate
{
	FGraph3DGroupIDsDelta::FGraph3DGroupIDsDelta()
	{ }

	FGraph3DGroupIDsDelta::FGraph3DGroupIDsDelta(const FGraph3DGroupIDsDelta &Other)
		: GroupIDsToAdd(Other.GroupIDsToAdd)
		, GroupIDsToRemove(Other.GroupIDsToRemove)
	{ }

	FGraph3DGroupIDsDelta::FGraph3DGroupIDsDelta(const TSet<int32> &InGroupIDsToAdd, const TSet<int32> &InGroupIDsToRemove)
		: GroupIDsToAdd(InGroupIDsToAdd)
		, GroupIDsToRemove(InGroupIDsToRemove)
	{ }

	FGraph3DGroupIDsDelta FGraph3DGroupIDsDelta::MakeInverse() const
	{
		return FGraph3DGroupIDsDelta(GroupIDsToRemove, GroupIDsToAdd);
	}


	FGraph3DFaceContainmentDelta::FGraph3DFaceContainmentDelta()
		: PrevContainingFaceID(MOD_ID_NONE)
		, NextContainingFaceID(MOD_ID_NONE)
	{ }

	FGraph3DFaceContainmentDelta::FGraph3DFaceContainmentDelta(const FGraph3DFaceContainmentDelta &Other)
		: PrevContainingFaceID(Other.PrevContainingFaceID)
		, NextContainingFaceID(Other.NextContainingFaceID)
		, ContainedFaceIDsToAdd(Other.ContainedFaceIDsToAdd)
		, ContainedFaceIDsToRemove(Other.ContainedFaceIDsToRemove)
	{
	}

	FGraph3DFaceContainmentDelta::FGraph3DFaceContainmentDelta(int32 InPrevContainingFaceID, int32 InNextContainingFaceID,
		const TSet<int32> &InContainedFaceIDsToAdd, const TSet<int32> &InContainedFaceIDsToRemove)
		: PrevContainingFaceID(InPrevContainingFaceID)
		, NextContainingFaceID(InNextContainingFaceID)
		, ContainedFaceIDsToAdd(InContainedFaceIDsToAdd)
		, ContainedFaceIDsToRemove(InContainedFaceIDsToRemove)
	{
	}

	FGraph3DFaceContainmentDelta FGraph3DFaceContainmentDelta::MakeInverse() const
	{
		return FGraph3DFaceContainmentDelta(
			NextContainingFaceID, PrevContainingFaceID,
			ContainedFaceIDsToRemove, ContainedFaceIDsToAdd);
	}


	FGraph3DObjDelta::FGraph3DObjDelta(const TArray<int32> &InVertices)
		: Vertices(InVertices)
	{
	}

	FGraph3DObjDelta::FGraph3DObjDelta(const TArray<int32> &InVertices, const TArray<int32> &InParents,
		const TSet<int32> &InGroupIDs, int32 InContainingObjID, const TArray<int32> &InContainedObjIDs)
		: Vertices(InVertices)
		, ParentObjIDs(InParents)
		, GroupIDs(InGroupIDs)
		, ContainingObjID(InContainingObjID)
		, ContainedObjIDs(InContainedObjIDs)
	{
	}

	FGraph3DObjDelta::FGraph3DObjDelta(const FVertexPair &VertexPair)
		: Vertices({ VertexPair.Key, VertexPair.Value })
	{
	}

	FGraph3DObjDelta::FGraph3DObjDelta(const FVertexPair &VertexPair, const TArray<int32> &InParents,
		const TSet<int32> &InGroupIDs)
		: Vertices({ VertexPair.Key, VertexPair.Value })
		, ParentObjIDs(InParents)
		, GroupIDs(InGroupIDs)
		, ContainingObjID(MOD_ID_NONE)
	{
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
		FaceContainmentUpdates.Reset();

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
		if (FaceContainmentUpdates.Num() > 0) return false;
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

	TSharedPtr<FGraph3DDelta> FGraph3DDelta::MakeGraphInverse() const
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

		for (const auto &kvp : FaceContainmentUpdates)
		{
			inverse->FaceContainmentUpdates.Add(kvp.Key, kvp.Value.MakeInverse());
		}

		inverse->FaceAdditions = FaceDeletions;
		inverse->FaceDeletions = FaceAdditions;

		for (const auto &kvp : ParentIDUpdates)
		{
			int32 parentID = kvp.Key;
			const FGraph3DHostedObjectDelta &idUpdate = kvp.Value;

			inverse->ParentIDUpdates.Add(parentID, FGraph3DHostedObjectDelta(idUpdate.PreviousHostedObjID, idUpdate.NextParentID, idUpdate.PreviousParentID));
		}

		for (const auto &kvp : GroupIDsUpdates)
		{
			inverse->GroupIDsUpdates.Add(kvp.Key, kvp.Value.MakeInverse());
		}

		return inverse;
	}

	TSharedPtr<FDelta> FGraph3DDelta::MakeInverse() const
	{
		return MakeGraphInverse();
	}

	bool FGraph3DDelta::ApplyTo(FModumateDocument *doc, UWorld *world) const
	{
		doc->ApplyGraph3DDelta(*this, world);
		return true;
	}
}
