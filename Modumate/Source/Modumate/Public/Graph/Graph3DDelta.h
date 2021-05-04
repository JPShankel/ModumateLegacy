// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateTypes.h"
#include "Graph/Graph3DTypes.h"
#include "DocumentManagement/DocumentDelta.h"

#include "Graph3DDelta.generated.h"


// A struct that describes a change to a graph object (currently edges and faces)
USTRUCT()
struct MODUMATE_API FGraph3DObjDelta
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Vertices;					// vertex IDs that define the positions for this object

	UPROPERTY()
	TArray<int32> ParentObjIDs;				// objects that were deleted to make this object

	UPROPERTY()
	TSet<int32> GroupIDs;					// group objects that this object is related to

	UPROPERTY()
	int32 ContainingObjID = MOD_ID_NONE;	// the ID of the object that contains this object, if any

	UPROPERTY()
	TSet<int32> ContainedObjIDs;			// the IDs of objects that are contained by this object

	FGraph3DObjDelta();
	FGraph3DObjDelta(const TArray<int32>& InVertices);
	FGraph3DObjDelta(const TArray<int32>& InVertices, const TArray<int32>& InParents,
		const TSet<int32>& InGroupIDs = TSet<int32>(),
		int32 InContainingObjID = MOD_ID_NONE, const TSet<int32>& InContainedObjIDs = TSet<int32>());

	FGraph3DObjDelta(const FGraphVertexPair& VertexPair);
	FGraph3DObjDelta(const FGraphVertexPair& VertexPair, const TArray<int32>& InParents,
		const TSet<int32>& InGroupIDs = TSet<int32>());
};

USTRUCT()
struct MODUMATE_API FGraph3DGroupIDsDelta
{
	GENERATED_BODY()

	UPROPERTY()
	TSet<int32> GroupIDsToAdd;

	UPROPERTY()
	TSet<int32> GroupIDsToRemove;

	FGraph3DGroupIDsDelta();
	FGraph3DGroupIDsDelta(const FGraph3DGroupIDsDelta& Other);
	FGraph3DGroupIDsDelta(const TSet<int32>& InGroupIDsToAdd, const TSet<int32>& InGroupIDsToRemove);

	FGraph3DGroupIDsDelta MakeInverse() const;
};

USTRUCT()
struct MODUMATE_API FGraph3DFaceContainmentDelta
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PrevContainingFaceID;

	UPROPERTY()
	int32 NextContainingFaceID;

	UPROPERTY()
	TSet<int32> ContainedFaceIDsToAdd;

	UPROPERTY()
	TSet<int32> ContainedFaceIDsToRemove;

	FGraph3DFaceContainmentDelta();
	FGraph3DFaceContainmentDelta(const FGraph3DFaceContainmentDelta& Other);
	FGraph3DFaceContainmentDelta(int32 InPrevContainingFaceID, int32 InNextContainingFaceID,
		const TSet<int32>& InContainedFaceIDsToAdd = TSet<int32>(),
		const TSet<int32>& InContainedFaceIDsToRemove = TSet<int32>());

	FGraph3DFaceContainmentDelta MakeInverse() const;
	bool IsEmpty() const;
};

USTRUCT()
struct MODUMATE_API FGraph3DFaceVertexIDsDelta
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> PrevVertexIDs;

	UPROPERTY()
	TArray<int32> NextVertexIDs;

	FGraph3DFaceVertexIDsDelta();
	FGraph3DFaceVertexIDsDelta(TArray<int32> InPrevVertexIDs, TArray<int32> InNextVertexIDs);

	FGraph3DFaceVertexIDsDelta MakeInverse() const;
};

// A struct that completely describes a change to the 3D graph
USTRUCT()
struct MODUMATE_API FGraph3DDelta : public FDocumentDelta
{
	GENERATED_BODY()

	virtual ~FGraph3DDelta() {}

	UPROPERTY()
	TMap<int32, FModumateVectorPair> VertexMovements;

	UPROPERTY()
	TMap<int32, FVector> VertexAdditions;

	UPROPERTY()
	TMap<int32, FVector> VertexDeletions;

	UPROPERTY()
	TMap<int32, FGraph3DObjDelta> EdgeAdditions;

	UPROPERTY()
	TMap<int32, FGraph3DObjDelta> EdgeDeletions;

	UPROPERTY()
	TMap<int32, FGraph3DObjDelta> FaceAdditions;

	UPROPERTY()
	TMap<int32, FGraph3DObjDelta> FaceDeletions;

	UPROPERTY()
	TMap<int32, FGraph3DFaceContainmentDelta> FaceContainmentUpdates;

	UPROPERTY()
	TMap<int32, FGraph3DFaceVertexIDsDelta> FaceVertexIDUpdates;

	// Updates to GroupIDs for graph objects
	UPROPERTY()
	TMap<int32, FGraph3DGroupIDsDelta> GroupIDsUpdates;

	void Reset();
	bool IsEmpty();

	int32 FindAddedVertex(const FVector& Position);
	int32 FindAddedEdge(const FGraphVertexPair& Edge);

	void AggregateDeletedObjects(TSet<int32>& OutDeletedObjIDs);

	TSharedPtr<FGraph3DDelta> MakeGraphInverse() const;
	virtual FDeltaPtr MakeInverse() const override;
	virtual bool ApplyTo(UModumateDocument *doc, UWorld *world) const override;
	virtual FStructDataWrapper SerializeStruct() override { return FStructDataWrapper(StaticStruct(), this, true); }
	virtual void GetAffectedObjects(TArray<TPair<int32, EMOIDeltaType>>& OutAffectedObjects) const override;
};
