// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/DocumentDelta.h"
#include "Graph/Graph2DTypes.h"
#include "ModumateCore/ModumateTypes.h"


// A struct that describes a change to a graph object (currently edges and faces)
struct FGraph2DObjDelta
{
	TArray<int32> Vertices;			// vertex IDs that define the positions for this object
	TArray<int32> ParentObjIDs;		// objects that were deleted to make this object

	// The 3D graph has group IDs and contained obj IDs, and those relationships could be useful here.
	// However, it is likely preferable to have the 3D graph maintain those relationships / features
	// and keep 2D graphs simple, but have more of them to approximate the behavior.

	FGraph2DObjDelta(const TArray<int32> &InVertices);
	FGraph2DObjDelta(const TArray<int32> &InVertices, const TArray<int32> &InParents);
};

enum EGraph2DDeltaType
{
	Add,
	Edit,
	Remove,
};

// TODO: this is redundant with FGraph3DHostedObjectDelta
struct FGraph2DHostedObjectDelta
{
	int32 PreviousHostedObjID;
	int32 PreviousParentID;
	int32 NextParentID;

	FGraph2DHostedObjectDelta(int32 prevHostedObjID, int32 prevParentID, int32 nextParentID)
		: PreviousHostedObjID(prevHostedObjID)
		, PreviousParentID(prevParentID)
		, NextParentID(nextParentID)
	{ }
};

struct FBoundsUpdate
{
	FBoundsUpdate();

	TPair<int32, TArray<int32>> OuterBounds;
	TMap<int32, TArray<int32>> InnerBounds;
	TMap<int32, int32> GraphFaceToInnerBound;
	TMap<int32, int32> GraphVerticesToSurfaceVertices;

	void Reset();
	bool IsEmpty() const;
};

// TODO: generalize FGraph3DHostedObjDelta for use here as well

// A struct that completely describes a change to the 2D graph
class FGraph2DDelta : public FDocumentDelta
{
public:
	int32 ID = MOD_ID_NONE; // id of the surface graph object to apply this delta

	EGraph2DDeltaType DeltaType = EGraph2DDeltaType::Edit;

	TMap<int32, TPair<FVector2D, FVector2D>> VertexMovements;
	TMap<int32, FVector2D> VertexAdditions;
	TMap<int32, FVector2D> VertexDeletions;

	TMap<int32, FGraph2DObjDelta> EdgeAdditions;
	TMap<int32, FGraph2DObjDelta> EdgeDeletions;

	TMap<int32, FGraph2DObjDelta> PolygonAdditions;
	TMap<int32, FGraph2DObjDelta> PolygonDeletions;

	TPair<FBoundsUpdate, FBoundsUpdate> BoundsUpdates;

	FGraph2DDelta(int32 InID, EGraph2DDeltaType InDeltaType = EGraph2DDeltaType::Edit);

	void Reset();
	bool IsEmpty() const;

	void AddNewVertex(const FVector2D& Position, int32& NextID);
	void AddNewEdge(const FGraphVertexPair& VertexIDs, int32& NextID, const TArray<int32>& ParentIDs = TArray<int32>());
	void AddNewPolygon(const TArray<int32>& VertexIDs, int32& NextID, const TArray<int32>& ParentIDs = TArray<int32>());

	void AggregateDeletedObjects(TSet<int32>& OutDeletedObjIDs);

	void Invert();
	TSharedPtr<FGraph2DDelta> MakeGraphInverse() const;
	virtual FDeltaPtr MakeInverse() const override;
	virtual bool ApplyTo(FModumateDocument *doc, UWorld *world) const override;
};
