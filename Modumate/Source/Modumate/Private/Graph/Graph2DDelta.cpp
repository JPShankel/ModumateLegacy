#include "Graph/Graph2DDelta.h"
#include "DocumentManagement/ModumateDocument.h"

namespace Modumate
{
	FGraph2DObjDelta::FGraph2DObjDelta(const TArray<int32> &InVertices)
		: Vertices(InVertices)
	{ }

	FGraph2DObjDelta::FGraph2DObjDelta(const TArray<int32> &InVertices, const TArray<int32> &InParents)
		: Vertices(InVertices)
		, ParentObjIDs(InParents)
	{ }

	FGraph2DObjDelta::FGraph2DObjDelta(const TArray<int32> &InVertices, const TArray<int32> &InParents, bool bIsInterior)
		: Vertices(InVertices)
		, ParentObjIDs(InParents)
		, bInterior(bIsInterior)
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

		return true;
	}

	void FGraph2DDelta::AddNewVertex(const FVector2D &Position, int32 &NextID)
	{
		int32 newVertexID = NextID++;
		VertexAdditions.Add(newVertexID, Position);
	}

	void FGraph2DDelta::AddNewEdge(const TPair<int32, int32> &VertexIDs, int32 &NextID, const TArray<int32> &ParentIDs)
	{
		int32 newEdgeID = NextID++;
		EdgeAdditions.Add(newEdgeID, FGraph2DObjDelta({ VertexIDs.Key, VertexIDs.Value }, ParentIDs));
	}

	void FGraph2DDelta::AddNewPolygon(const TArray<int32> &VertexIDs, int32 &NextID, bool bIsInterior, const TArray<int32> &ParentIDs)
	{
		int32 newPolygonID = NextID++;
		PolygonAdditions.Add(newPolygonID, FGraph2DObjDelta(VertexIDs, ParentIDs, bIsInterior));
	}

	TSharedPtr<FGraph2DDelta> FGraph2DDelta::MakeGraphInverse() const
	{
		TSharedPtr<FGraph2DDelta> inverse = MakeShareable(new FGraph2DDelta(ID));

		switch (DeltaType)
		{
		case EGraph2DDeltaType::Add:
			inverse->DeltaType = EGraph2DDeltaType::Remove;
			return inverse;
			break;
		case EGraph2DDeltaType::Edit:
			inverse->DeltaType = EGraph2DDeltaType::Edit;
			break;
		case EGraph2DDeltaType::Remove:
			inverse->DeltaType = EGraph2DDeltaType::Add;
			break;
		default:
			break;
		}

		for (const auto &kvp : VertexMovements)
		{
			int32 vertexID = kvp.Key;
			const TPair<FVector2D, FVector2D> &vertexMovement = kvp.Value;

			inverse->VertexMovements.Add(vertexID, TPair<FVector2D, FVector2D>(vertexMovement.Value, vertexMovement.Key));
		}

		inverse->VertexAdditions = VertexDeletions;
		inverse->VertexDeletions = VertexAdditions;

		inverse->EdgeAdditions = EdgeDeletions;
		inverse->EdgeDeletions = EdgeAdditions;

		inverse->PolygonAdditions = PolygonDeletions;
		inverse->PolygonDeletions = PolygonAdditions;

		return inverse;
	}

	TSharedPtr<FDelta> FGraph2DDelta::MakeInverse() const
	{
		return MakeGraphInverse();
	}

	bool FGraph2DDelta::ApplyTo(FModumateDocument *doc, UWorld *world) const
	{
		doc->ApplyGraph2DDelta(*this, world);
		return true;
	}
}
