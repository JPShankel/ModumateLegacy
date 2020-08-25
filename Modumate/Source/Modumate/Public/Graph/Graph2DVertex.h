#include "Graph/Graph2DTypes.h"

#pragma once

namespace Modumate
{
	class FGraph2DVertex : public IGraph2DObject
	{
	public:
		// Definitional data
		FVector2D Position = FVector2D::ZeroVector;		// The position of the vertex

		// Derived data
		TArray<FGraphSignedID> Edges;					// The list of edges (sorted, clockwise, from +X) connected to this vertex

		FGraph2DVertex(int32 InID, FGraph2D* InGraph, const FVector2D &InPos);

		void SetPosition(const FVector2D& NewPosition);
		bool AddEdge(FGraphSignedID EdgeID);
		bool RemoveEdge(FGraphSignedID EdgeID);
		bool GetNextEdge(FGraphSignedID CurEdgeID, FGraphSignedID &OutNextEdgeID, float &OutAngleDelta,
			const TSet<FGraphSignedID>* AllowedEdgeIDs = nullptr, bool bForwards = true) const;

		virtual void Dirty(bool bConnected = true) override;
		virtual bool Clean() override;

		virtual EGraphObjectType GetType() const override { return EGraphObjectType::Vertex; };
		virtual void GetVertexIDs(TArray<int32> &OutVertexIDs) const override;

	protected:
		bool SortEdges();
	};
}
