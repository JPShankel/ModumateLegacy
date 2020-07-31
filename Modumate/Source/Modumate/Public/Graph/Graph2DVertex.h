#include "Graph/Graph2DTypes.h"

#pragma once

namespace Modumate
{
	class FGraph2DVertex : public IGraph2DObject
	{
	public:
		FVector2D Position = FVector2D::ZeroVector;	// The position of the vertex
		TArray<FGraphSignedID> Edges;						// The list of edges (sorted, clockwise, from +X) connected to this vertex

		FGraph2DVertex(int32 InID, FGraph2D* InGraph, const FVector2D &InPos)
			: IGraph2DObject(InID, InGraph)
			, Position(InPos)
		{ }

		void AddEdge(FGraphSignedID EdgeID);
		bool RemoveEdge(FGraphSignedID EdgeID);
		void SortEdges();
		bool GetNextEdge(FGraphSignedID curEdgeID, FGraphSignedID &outNextEdgeID, float &outAngleDelta, int32 indexDelta = 1) const;

		virtual void Dirty(bool bConnected = true) override;
		virtual void Clean() override;

		virtual EGraphObjectType GetType() const override { return EGraphObjectType::Vertex; };
	};
}
