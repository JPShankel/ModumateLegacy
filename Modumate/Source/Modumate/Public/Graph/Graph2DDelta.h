// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DocumentManagement/DocumentDelta.h"
#include "Graph/Graph2DTypes.h"
#include "ModumateCore/ModumateTypes.h"

#include "Graph2DDelta.generated.h"

// A struct that describes a change to a graph object (currently edges and faces)
USTRUCT()
struct FGraph2DObjDelta
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Vertices;			// vertex IDs that define the positions for this object

	UPROPERTY()
	TArray<int32> ParentObjIDs;		// objects that were deleted to make this object

	// The 3D graph has group IDs and contained obj IDs, and those relationships could be useful here.
	// However, it is likely preferable to have the 3D graph maintain those relationships / features
	// and keep 2D graphs simple, but have more of them to approximate the behavior.

	FGraph2DObjDelta();
	FGraph2DObjDelta(const TArray<int32> &InVertices);
	FGraph2DObjDelta(const TArray<int32> &InVertices, const TArray<int32> &InParents);
};

UENUM()
enum class EGraph2DDeltaType
{
	Add,
	Edit,
	Remove,
};

USTRUCT()
struct MODUMATE_API FGraph2DFaceVertexIDsDelta
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> PrevVertexIDs;

	UPROPERTY()
	TArray<int32> NextVertexIDs;

	FGraph2DFaceVertexIDsDelta();
	FGraph2DFaceVertexIDsDelta(const TArray<int32> &InPrevVertexIDs, const TArray<int32> &InNextVertexIDs);

	FGraph2DFaceVertexIDsDelta MakeInverse() const;
};

// TODO: generalize FGraph3DHostedObjDelta for use here as well

// A struct that completely describes a change to the 2D graph
USTRUCT()
struct MODUMATE_API FGraph2DDelta : public FDocumentDelta
{
	GENERATED_BODY()

	virtual ~FGraph2DDelta() {}

	UPROPERTY()
	int32 ID = MOD_ID_NONE; // id of the surface graph object to apply this delta

	UPROPERTY()
	EGraph2DDeltaType DeltaType = EGraph2DDeltaType::Edit;

	UPROPERTY()
	float Epsilon = DEFAULT_GRAPH_EPSILON;

	UPROPERTY()
	TMap<int32, FVector2DPair> VertexMovements;

	UPROPERTY()
	TMap<int32, FVector2D> VertexAdditions;

	UPROPERTY()
	TMap<int32, FVector2D> VertexDeletions;

	UPROPERTY()
	TMap<int32, FGraph2DObjDelta> EdgeAdditions;

	UPROPERTY()
	TMap<int32, FGraph2DObjDelta> EdgeDeletions;

	UPROPERTY()
	TMap<int32, FGraph2DObjDelta> PolygonAdditions;

	UPROPERTY()
	TMap<int32, FGraph2DObjDelta> PolygonDeletions;

	UPROPERTY()
	TMap<int32, FGraph2DFaceVertexIDsDelta> PolygonIDUpdates;

	FGraph2DDelta();
	FGraph2DDelta(int32 InID, EGraph2DDeltaType InDeltaType = EGraph2DDeltaType::Edit, float InEpsilon = DEFAULT_GRAPH_EPSILON);

	void Reset();
	bool IsEmpty() const;

	void AddNewVertex(const FVector2D& Position, int32& NextID);
	void AddNewEdge(const FGraphVertexPair& VertexIDs, int32& NextID, const TArray<int32>& ParentIDs = TArray<int32>());
	void AddNewPolygon(const TArray<int32>& VertexIDs, int32& NextID, const TArray<int32>& ParentIDs = TArray<int32>());

	void AggregateDeletedObjects(TSet<int32>& OutDeletedObjIDs);

	void Invert();
	TSharedPtr<FGraph2DDelta> MakeGraphInverse() const;
	virtual FDeltaPtr MakeInverse() const override;
	virtual bool ApplyTo(UModumateDocument *doc, UWorld *world) const override;
	virtual FStructDataWrapper SerializeStruct() override { return FStructDataWrapper(StaticStruct(), this, true); }
	virtual void GetAffectedObjects(TArray<TPair<int32, EMOIDeltaType>>& OutAffectedObjects) const override;
};
