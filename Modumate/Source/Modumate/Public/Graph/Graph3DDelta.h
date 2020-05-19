// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateTypes.h"
#include "Graph/ModumateGraph3DTypes.h"
#include "DocumentManagement/ModumateDelta.h"

namespace Modumate
{
	// A struct that describes a change to a graph object (currently edges and faces)
	struct FGraph3DObjDelta
	{
		TArray<int32> Vertices;		// vertex IDs that define the positions for this object
		TArray<int32> ParentObjIDs; // objects that were deleted to make this object
		TSet<int32> GroupIDs;		// group objects that this object is related to

		FGraph3DObjDelta(const TArray<int32> &InVertices);
		FGraph3DObjDelta(const TArray<int32> &InVertices, const TArray<int32> &InParents, const TSet<int32> &InGroupIDs = TSet<int32>());

		FGraph3DObjDelta(const FVertexPair &VertexPair);
		FGraph3DObjDelta(const FVertexPair &VertexPair, const TArray<int32> &InParents, const TSet<int32> &InGroupIDs = TSet<int32>());
	};

	struct FGraph3DHostedObjectDelta
	{
		int32 PreviousHostedObjID;
		int32 PreviousParentID;
		int32 NextParentID;

		FGraph3DHostedObjectDelta(int32 prevHostedObjID, int32 prevParentID, int32 nextParentID)
			: PreviousHostedObjID(prevHostedObjID)
			, PreviousParentID(prevParentID)
			, NextParentID(nextParentID)
		{ }
	};

	struct FGraph3DGroupIDsDelta
	{
		TSet<int32> GroupIDsToAdd, GroupIDsToRemove;

		FGraph3DGroupIDsDelta() { }

		FGraph3DGroupIDsDelta(const FGraph3DGroupIDsDelta &Other)
			: GroupIDsToAdd(Other.GroupIDsToAdd)
			, GroupIDsToRemove(Other.GroupIDsToRemove)
		{ }

		FGraph3DGroupIDsDelta(const TSet<int32> &InGroupIDsToAdd, const TSet<int32> &InGroupIDsToRemove)
			: GroupIDsToAdd(InGroupIDsToAdd)
			, GroupIDsToRemove(InGroupIDsToRemove)
		{ }

		FGraph3DGroupIDsDelta MakeInverse() const;
	};

	// A struct that completely describes a change to the 3D graph
	class FGraph3DDelta : public FDelta
	{
	public:
		TMap<int32, TPair<FVector, FVector>> VertexMovements;
		TMap<int32, FVector> VertexAdditions;
		TMap<int32, FVector> VertexDeletions;

		TMap<int32, FGraph3DObjDelta> EdgeAdditions;
		TMap<int32, FGraph3DObjDelta> EdgeDeletions;

		TMap<int32, FGraph3DObjDelta> FaceAdditions;
		TMap<int32, FGraph3DObjDelta> FaceDeletions;

		// map from faceID to a map from vertex index to new ID
		TMap<int32, TMap<int32, int32>> FaceVertexAdditions;
		TMap<int32, TMap<int32, int32>> FaceVertexRemovals;

		TMap<int32, FGraph3DHostedObjectDelta> ParentIDUpdates;

		// Updates to GroupIDs for graph objects
		TMap<FTypedGraphObjID, FGraph3DGroupIDsDelta> GroupIDsUpdates;

		void Reset();
		bool IsEmpty();

		int32 FindAddedVertex(FVector position);
		int32 FindAddedEdge(FVertexPair edge);

		void AggregateAddedObjects(TArray<int32> OutAddedFaceIDs, TArray<int32> OutAddedVertexIDs, TArray<int32> OutAddedEdgeIDs);

		virtual TSharedPtr<FDelta> MakeInverse() const override;
		virtual bool ApplyTo(FModumateDocument *doc, UWorld *world) const override;
	};
}
