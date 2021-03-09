#include "Graph/Graph2DDelta.h"
#include "DocumentManagement/ModumateDocument.h"


FGraph2DObjDelta::FGraph2DObjDelta()
{ }

FGraph2DObjDelta::FGraph2DObjDelta(const TArray<int32>& InVertices)
	: Vertices(InVertices)
{ }

FGraph2DObjDelta::FGraph2DObjDelta(const TArray<int32>& InVertices, const TArray<int32>& InParents)
	: Vertices(InVertices)
	, ParentObjIDs(InParents)
{ }

FGraph2DFaceVertexIDsDelta::FGraph2DFaceVertexIDsDelta()
{

}

FGraph2DFaceVertexIDsDelta::FGraph2DFaceVertexIDsDelta(const TArray<int32> &InPrevVertexIDs, const TArray<int32> &InNextVertexIDs)
	: PrevVertexIDs(InPrevVertexIDs)
	, NextVertexIDs(InNextVertexIDs)
{

}

FGraph2DFaceVertexIDsDelta FGraph2DFaceVertexIDsDelta::MakeInverse() const
{
	return FGraph2DFaceVertexIDsDelta(NextVertexIDs, PrevVertexIDs);
}

FGraph2DDelta::FGraph2DDelta()
{ }

FGraph2DDelta::FGraph2DDelta(int32 InID, EGraph2DDeltaType InDeltaType)
	: ID(InID)
	, DeltaType(InDeltaType)
{
}

void FGraph2DDelta::Reset()
{
	VertexMovements.Reset();
	VertexAdditions.Reset();
	VertexDeletions.Reset();

	EdgeAdditions.Reset();
	EdgeDeletions.Reset();

	PolygonAdditions.Reset();
	PolygonDeletions.Reset();
	PolygonIDUpdates.Reset();
}

bool FGraph2DDelta::IsEmpty() const
{
	if (VertexMovements.Num() > 0) return false;
	if (VertexAdditions.Num() > 0) return false;
	if (VertexDeletions.Num() > 0) return false;
	if (EdgeAdditions.Num() > 0) return false;
	if (EdgeDeletions.Num() > 0) return false;
	if (PolygonAdditions.Num() > 0) return false;
	if (PolygonDeletions.Num() > 0) return false;
	if (PolygonIDUpdates.Num() > 0) return false;

	return true;
}

void FGraph2DDelta::AddNewVertex(const FVector2D& Position, int32& NextID)
{
	int32 newVertexID = NextID++;
	VertexAdditions.Add(newVertexID, Position);
}

void FGraph2DDelta::AddNewEdge(const FGraphVertexPair& VertexIDs, int32& NextID, const TArray<int32>& ParentIDs)
{
	int32 newEdgeID = NextID++;
	EdgeAdditions.Add(newEdgeID, FGraph2DObjDelta({ VertexIDs.Key, VertexIDs.Value }, ParentIDs));
}

void FGraph2DDelta::AddNewPolygon(const TArray<int32>& VertexIDs, int32& NextID, const TArray<int32>& ParentIDs)
{
	int32 newPolygonID = NextID++;
	PolygonAdditions.Add(newPolygonID, FGraph2DObjDelta(VertexIDs, ParentIDs));
}

void FGraph2DDelta::AggregateDeletedObjects(TSet<int32>& OutDeletedObjIDs)
{
	for (auto& kvp : VertexDeletions)
	{
		OutDeletedObjIDs.Add(kvp.Key);
	}
	for (auto& kvp : EdgeDeletions)
	{
		OutDeletedObjIDs.Add(kvp.Key);
	}
	for (auto& kvp : PolygonDeletions)
	{
		OutDeletedObjIDs.Add(kvp.Key);
	}

	if (DeltaType == EGraph2DDeltaType::Remove)
	{
		OutDeletedObjIDs.Add(ID);
	}
}

void FGraph2DDelta::Invert()
{
	switch (DeltaType)
	{
	case EGraph2DDeltaType::Add:
		DeltaType = EGraph2DDeltaType::Remove;
		break;
	case EGraph2DDeltaType::Remove:
		DeltaType = EGraph2DDeltaType::Add;
		break;
	default:
		break;
	}

	for (auto& kvp : VertexMovements)
	{
		FVector2DPair& vertexMovement = kvp.Value;
		Swap(vertexMovement.Key, vertexMovement.Value);
	}

	Swap(VertexAdditions, VertexDeletions);
	Swap(EdgeAdditions, EdgeDeletions);
	Swap(PolygonAdditions, PolygonDeletions);

	for (auto& kvp : PolygonIDUpdates)
	{
		PolygonIDUpdates[kvp.Key] = kvp.Value.MakeInverse();
	}
}

TSharedPtr<FGraph2DDelta> FGraph2DDelta::MakeGraphInverse() const
{
	auto inverse = MakeShared<FGraph2DDelta>(*this);
	inverse->Invert();
	return inverse;
}

FDeltaPtr FGraph2DDelta::MakeInverse() const
{
	return MakeGraphInverse();
}

bool FGraph2DDelta::ApplyTo(UModumateDocument* doc, UWorld* world) const
{
	doc->ApplyGraph2DDelta(*this, world);
	return true;
}
