// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateTypes.h"
#include "ModumateGraph3DTypes.h"
#include "ModumateDelta.h"

namespace Modumate
{
	// A struct that describes a change to a graph object (currently edges and faces)
	struct FGraph3DObjDelta
	{
		TArray<int32> Vertices;		// vertex IDs that define the positions for this object
		TArray<int32> ParentObjIDs; // objects that were deleted to make this object
		TSet<int32> GroupIDs;		// group objects that this object is related to

		FGraph3DObjDelta(TArray<int32> vertices) : Vertices(vertices) {}
		FGraph3DObjDelta(TArray<int32> vertices, TArray<int32> parents) : Vertices(vertices), ParentObjIDs(parents) {}

		FGraph3DObjDelta(FVertexPair vertexPair);
		FGraph3DObjDelta(FVertexPair vertexPair, TArray<int32> parents);
	};

	struct FGraph3DHostedObjectDelta
	{
		int32 PreviousHostedObjID;
		int32 PreviousParentID;
		int32 NextParentID;

		FGraph3DHostedObjectDelta(int32 prevHostedObjID, int32 prevParentID, int32 nextParentID)
			: PreviousHostedObjID(prevHostedObjID),
			PreviousParentID(prevParentID),
			NextParentID(nextParentID) {}
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

		void Reset();
		bool IsEmpty();

		int32 FindAddedVertex(FVector position);
		int32 FindAddedEdge(FVertexPair edge);

		void AggregateAddedObjects(TArray<int32> OutAddedFaceIDs, TArray<int32> OutAddedVertexIDs, TArray<int32> OutAddedEdgeIDs);

		virtual TSharedPtr<FDelta> MakeInverse() const override;
		virtual bool ApplyTo(FModumateDocument *doc, UWorld *world) const override;
	};
}
