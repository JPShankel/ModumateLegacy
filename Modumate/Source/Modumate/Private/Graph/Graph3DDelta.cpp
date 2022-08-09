// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph3DDelta.h"
#include "Objects/ModumateObjectEnums.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectDeltaStatics.h"


FGraph3DFaceContainmentDelta::FGraph3DFaceContainmentDelta()
	: PrevContainingFaceID(MOD_ID_NONE)
	, NextContainingFaceID(MOD_ID_NONE)
{ }

FGraph3DFaceContainmentDelta::FGraph3DFaceContainmentDelta(const FGraph3DFaceContainmentDelta& Other)
	: PrevContainingFaceID(Other.PrevContainingFaceID)
	, NextContainingFaceID(Other.NextContainingFaceID)
	, ContainedFaceIDsToAdd(Other.ContainedFaceIDsToAdd)
	, ContainedFaceIDsToRemove(Other.ContainedFaceIDsToRemove)
{
}

FGraph3DFaceContainmentDelta::FGraph3DFaceContainmentDelta(int32 InPrevContainingFaceID, int32 InNextContainingFaceID,
	const TSet<int32>& InContainedFaceIDsToAdd, const TSet<int32>& InContainedFaceIDsToRemove)
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

bool FGraph3DFaceContainmentDelta::IsEmpty() const
{
	return (PrevContainingFaceID == NextContainingFaceID) &&
		(ContainedFaceIDsToAdd.Num() == 0) &&
		(ContainedFaceIDsToRemove.Num() == 0);
}

FGraph3DFaceVertexIDsDelta::FGraph3DFaceVertexIDsDelta()
{

}

FGraph3DFaceVertexIDsDelta::FGraph3DFaceVertexIDsDelta(TArray<int32> InPrevVertexIDs, TArray<int32> InNextVertexIDs)
	: PrevVertexIDs(InPrevVertexIDs)
	, NextVertexIDs(InNextVertexIDs)
{

}

FGraph3DFaceVertexIDsDelta FGraph3DFaceVertexIDsDelta::MakeInverse() const
{
	return FGraph3DFaceVertexIDsDelta(NextVertexIDs, PrevVertexIDs);
}

FGraph3DObjDelta::FGraph3DObjDelta()
{

}

FGraph3DObjDelta::FGraph3DObjDelta(const TArray<int32>& InVertices)
	: Vertices(InVertices)
{
}

FGraph3DObjDelta::FGraph3DObjDelta(const TArray<int32>& InVertices, const TArray<int32>& InParents,
	int32 InContainingObjID, const TSet<int32>& InContainedObjIDs)
	: Vertices(InVertices)
	, ParentObjIDs(InParents)
	, ContainingObjID(InContainingObjID)
	, ContainedObjIDs(InContainedObjIDs)
{
}

FGraph3DObjDelta::FGraph3DObjDelta(const FGraphVertexPair& VertexPair)
	: Vertices({ VertexPair.Key, VertexPair.Value })
{
}

FGraph3DObjDelta::FGraph3DObjDelta(const FGraphVertexPair& VertexPair, const TArray<int32>& InParents)
	: Vertices({ VertexPair.Key, VertexPair.Value })
	, ParentObjIDs(InParents)
	, ContainingObjID(MOD_ID_NONE)
{
}

FGraph3DDelta::FGraph3DDelta(int32 InID /*= MOD_ID_NONE*/)
	: GraphID(InID)
{ }

void FGraph3DDelta::Reset()
{
	VertexMovements.Reset();
	VertexAdditions.Reset();
	VertexDeletions.Reset();

	EdgeAdditions.Reset();
	EdgeDeletions.Reset();
	EdgeReversals.Reset();

	FaceAdditions.Reset();
	FaceDeletions.Reset();
	FaceContainmentUpdates.Reset();
	FaceReversals.Reset();

	FaceVertexIDUpdates.Reset();
}

bool FGraph3DDelta::IsEmpty()
{
	// TODO: check for no-op deltas for all types, not just FaceContainmentUpdates
	if (VertexMovements.Num() > 0) return false;
	if (VertexAdditions.Num() > 0) return false;
	if (VertexDeletions.Num() > 0) return false;
	if (EdgeAdditions.Num() > 0) return false;
	if (EdgeDeletions.Num() > 0) return false;
	if (EdgeReversals.Num() > 0) return false;
	if (FaceAdditions.Num() > 0) return false;
	if (FaceDeletions.Num() > 0) return false;
	if (FaceReversals.Num() > 0) return false;
	for (auto& kvp : FaceContainmentUpdates)
	{
		if (!kvp.Value.IsEmpty())
		{
			return false;
		}
	}
	if (FaceVertexIDUpdates.Num() > 0) return false;

	return true;
}

int32 FGraph3DDelta::FindAddedVertex(const FVector& Position)
{
	for (auto& kvp : VertexAdditions)
	{
		FVector otherVertex = kvp.Value;
		if (Position.Equals(otherVertex, DEFAULT_GRAPH3D_EPSILON))
		{
			return kvp.Key;
		}
	}

	return MOD_ID_NONE;
}

int32 FGraph3DDelta::FindAddedEdge(const FGraphVertexPair& Edge)
{
	FGraphVertexPair flippedEdge(Edge.Value, Edge.Key);
	for (auto& kvp : EdgeAdditions)
	{
		FGraphVertexPair otherPair(kvp.Value.Vertices[0], kvp.Value.Vertices[1]);
		if (otherPair == Edge)
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

void FGraph3DDelta::AggregateDeletedObjects(TSet<int32>& OutDeletedObjIDs)
{
	for (auto& kvp : VertexDeletions)
	{
		OutDeletedObjIDs.Add(kvp.Key);
	}
	for (auto& kvp : EdgeDeletions)
	{
		OutDeletedObjIDs.Add(kvp.Key);
	}
	for (auto& kvp : FaceDeletions)
	{
		OutDeletedObjIDs.Add(kvp.Key);
	}
}

TSharedPtr<FGraph3DDelta> FGraph3DDelta::MakeGraphInverse() const
{
	auto inverse = MakeShared<FGraph3DDelta>(GraphID);

	if (DeltaType == EGraph3DDeltaType::Edit)
	{
		for (const auto& kvp : VertexMovements)
		{
			int32 vertexID = kvp.Key;
			const FModumateVectorPair& vertexMovement = kvp.Value;

			inverse->VertexMovements.Add(vertexID, { vertexMovement.Value, vertexMovement.Key });
		}

		inverse->VertexAdditions = VertexDeletions;
		inverse->VertexDeletions = VertexAdditions;

		inverse->EdgeAdditions = EdgeDeletions;
		inverse->EdgeDeletions = EdgeAdditions;

		inverse->EdgeReversals = EdgeReversals;
		inverse->FaceReversals = FaceReversals;

		for (const auto& kvp : FaceVertexIDUpdates)
		{
			inverse->FaceVertexIDUpdates.Add(kvp.Key, kvp.Value.MakeInverse());
		}

		for (const auto& kvp : FaceContainmentUpdates)
		{
			inverse->FaceContainmentUpdates.Add(kvp.Key, kvp.Value.MakeInverse());
		}

		inverse->FaceAdditions = FaceDeletions;
		inverse->FaceDeletions = FaceAdditions;
	}
	else
	{
		*inverse = *this;
		inverse->DeltaType = DeltaType == EGraph3DDeltaType::Add ? EGraph3DDeltaType::Remove : EGraph3DDeltaType::Add;
	}

	return inverse;
}

FDeltaPtr FGraph3DDelta::MakeInverse() const
{
	return MakeGraphInverse();
}

bool FGraph3DDelta::ApplyTo(UModumateDocument* doc, UWorld* world) const
{
	doc->ApplyGraph3DDelta(*this, world);
	return true;
}

void FGraph3DDelta::GetAffectedObjects(TArray<TPair<int32, EMOIDeltaType>>& OutAffectedObjects) const
{
	Super::GetAffectedObjects(OutAffectedObjects);

	AddAffectedIDs(VertexMovements, EMOIDeltaType::Mutate, OutAffectedObjects);
	AddAffectedIDs(VertexAdditions, EMOIDeltaType::Create, OutAffectedObjects);
	AddAffectedIDs(VertexDeletions, EMOIDeltaType::Destroy, OutAffectedObjects);
	AddAffectedIDs(EdgeAdditions, EMOIDeltaType::Create, OutAffectedObjects);
	AddAffectedIDs(EdgeDeletions, EMOIDeltaType::Destroy, OutAffectedObjects);
	AddAffectedIDs(FaceAdditions, EMOIDeltaType::Create, OutAffectedObjects);
	AddAffectedIDs(FaceDeletions, EMOIDeltaType::Destroy, OutAffectedObjects);
	AddAffectedIDs(FaceContainmentUpdates, EMOIDeltaType::Mutate, OutAffectedObjects);
	AddAffectedIDs(FaceVertexIDUpdates, EMOIDeltaType::Mutate, OutAffectedObjects);
	AddAffectedIDs(EdgeReversals, EMOIDeltaType::Mutate, OutAffectedObjects);
	AddAffectedIDs(FaceReversals, EMOIDeltaType::Mutate, OutAffectedObjects);
}

void FGraph3DDelta::GetDerivedDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaOperation, TArray<TSharedPtr<FDocumentDelta>>& OutDeltas) const
{
	if (DeltaOperation == EMOIDeltaType::Mutate)
	{
		FModumateObjectDeltaStatics::GetDerivedDeltasForGraph3d(Doc, this, DeltaOperation, OutDeltas);
	}
}
