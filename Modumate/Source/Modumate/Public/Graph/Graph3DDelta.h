// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateTypes.h"
#include "Graph/Graph3DTypes.h"
#include "DocumentManagement/DocumentDelta.h"


// A struct that describes a change to a graph object (currently edges and faces)
struct FGraph3DObjDelta
{
	TArray<int32> Vertices;					// vertex IDs that define the positions for this object
	TArray<int32> ParentObjIDs;				// objects that were deleted to make this object
	TSet<int32> GroupIDs;					// group objects that this object is related to
	int32 ContainingObjID = MOD_ID_NONE;	// the ID of the object that contains this object, if any
	TSet<int32> ContainedObjIDs;			// the IDs of objects that are contained by this object

	FGraph3DObjDelta(const TArray<int32>& InVertices);
	FGraph3DObjDelta(const TArray<int32>& InVertices, const TArray<int32>& InParents,
		const TSet<int32>& InGroupIDs = TSet<int32>(),
		int32 InContainingObjID = MOD_ID_NONE, const TSet<int32>& InContainedObjIDs = TSet<int32>());

	FGraph3DObjDelta(const FGraphVertexPair& VertexPair);
	FGraph3DObjDelta(const FGraphVertexPair& VertexPair, const TArray<int32>& InParents,
		const TSet<int32>& InGroupIDs = TSet<int32>());
};

struct FGraph3DGroupIDsDelta
{
	TSet<int32> GroupIDsToAdd, GroupIDsToRemove;

	FGraph3DGroupIDsDelta();
	FGraph3DGroupIDsDelta(const FGraph3DGroupIDsDelta& Other);
	FGraph3DGroupIDsDelta(const TSet<int32>& InGroupIDsToAdd, const TSet<int32>& InGroupIDsToRemove);

	FGraph3DGroupIDsDelta MakeInverse() const;
};

struct FGraph3DFaceContainmentDelta
{
	int32 PrevContainingFaceID, NextContainingFaceID;
	TSet<int32> ContainedFaceIDsToAdd, ContainedFaceIDsToRemove;

	FGraph3DFaceContainmentDelta();
	FGraph3DFaceContainmentDelta(const FGraph3DFaceContainmentDelta& Other);
	FGraph3DFaceContainmentDelta(int32 InPrevContainingFaceID, int32 InNextContainingFaceID,
		const TSet<int32>& InContainedFaceIDsToAdd = TSet<int32>(),
		const TSet<int32>& InContainedFaceIDsToRemove = TSet<int32>());

	FGraph3DFaceContainmentDelta MakeInverse() const;
};

// A struct that completely describes a change to the 3D graph
class FGraph3DDelta : public FDocumentDelta
{
public:
	TMap<int32, TPair<FVector, FVector>> VertexMovements;
	TMap<int32, FVector> VertexAdditions;
	TMap<int32, FVector> VertexDeletions;

	TMap<int32, FGraph3DObjDelta> EdgeAdditions;
	TMap<int32, FGraph3DObjDelta> EdgeDeletions;

	TMap<int32, FGraph3DObjDelta> FaceAdditions;
	TMap<int32, FGraph3DObjDelta> FaceDeletions;
	TMap<int32, FGraph3DFaceContainmentDelta> FaceContainmentUpdates;

	// map from faceID to a map from vertex index to new ID
	TMap<int32, TMap<int32, int32>> FaceVertexAdditions;
	TMap<int32, TMap<int32, int32>> FaceVertexRemovals;

	// Updates to GroupIDs for graph objects
	TMap<int32, FGraph3DGroupIDsDelta> GroupIDsUpdates;

	void Reset();
	bool IsEmpty();

	int32 FindAddedVertex(const FVector& Position);
	int32 FindAddedEdge(const FGraphVertexPair& Edge);

	void AggregateDeletedObjects(TSet<int32>& OutDeletedObjIDs);

	TSharedPtr<FGraph3DDelta> MakeGraphInverse() const;
	virtual FDeltaPtr MakeInverse() const override;
	virtual bool ApplyTo(FModumateDocument *doc, UWorld *world) const override;
};
