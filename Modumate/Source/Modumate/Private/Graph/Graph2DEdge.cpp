// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DEdge.h"

#include "Graph/Graph2D.h"

namespace Modumate
{
	FGraph2DEdge::FGraph2DEdge(int32 InID, FGraph2D* InGraph, int32 InStart, int32 InEnd)
		: IGraph2DObject(InID, InGraph)
	{
		SetVertices(InStart, InEnd);
	}

	void FGraph2DEdge::SetVertices(int32 InStart, int32 InEnd)
	{
		if (StartVertexID != MOD_ID_NONE)
		{
			auto startVertex = Graph->FindVertex(StartVertexID);
			ensureAlways(startVertex && startVertex->RemoveEdge(ID));
		}

		if (EndVertexID != MOD_ID_NONE)
		{
			auto endVertex = Graph->FindVertex(EndVertexID);
			ensureAlways(endVertex && endVertex->RemoveEdge(-ID));
		}

		StartVertexID = InStart;
		EndVertexID = InEnd;
		bValid = (StartVertexID != MOD_ID_NONE) && (EndVertexID != MOD_ID_NONE);
		Dirty(false);

		if (StartVertexID != MOD_ID_NONE)
		{
			auto startVertex = Graph->FindVertex(StartVertexID);
			ensureAlways(startVertex && startVertex->AddEdge(ID));
		}

		if (EndVertexID != MOD_ID_NONE)
		{
			auto endVertex = Graph->FindVertex(EndVertexID);
			ensureAlways(endVertex && endVertex->AddEdge(-ID));
		}
	}

	void FGraph2DEdge::Dirty(bool bConnected)
	{
		IGraph2DObject::Dirty(bConnected);
		bPolygonDirty = true;

		if (bConnected)
		{
			bool bContinueConnected = false;

			auto startVertex = Graph->FindVertex(StartVertexID);
			if (startVertex != nullptr)
			{
				startVertex->Dirty(bContinueConnected);
			}

			auto endVertex = Graph->FindVertex(EndVertexID);
			if (endVertex != nullptr)
			{
				endVertex->Dirty(bContinueConnected);
			}

			auto leftPoly = Graph->FindPolygon(LeftPolyID);
			if (leftPoly != nullptr)
			{
				leftPoly->Dirty(bContinueConnected);
			}

			auto rightPoly = Graph->FindPolygon(RightPolyID);
			if (rightPoly != nullptr)
			{
				rightPoly->Dirty(bContinueConnected);
			}
		}
	}

	bool FGraph2DEdge::Clean()
	{
		if (!bDerivedDataDirty)
		{
			return false;
		}

		if (ensure(CacheAngle()))
		{
			bDerivedDataDirty = false;
		}

		// NOTE: bPolygonDirty is not cleared here, but rather by FGraph2D::CalculatePolygons,
		// so that all graph objects' geometric data can be cleaned in the correct order before re-traversing polygons.

		return true;
	}

	void FGraph2DEdge::GetVertexIDs(TArray<int32> &OutVertexIDs) const
	{
		OutVertexIDs = { StartVertexID, EndVertexID };
	}

	bool FGraph2DEdge::CacheAngle()
	{
		CachedAngle = 0.0f;
		CachedEdgeDir = FVector2D::ZeroVector;

		if (ensureAlways(Graph) && (StartVertexID != 0) && (EndVertexID != 0))
		{
			FGraph2DVertex *startVertex = Graph->FindVertex(StartVertexID);
			FGraph2DVertex *endVertex = Graph->FindVertex(EndVertexID);
			if (ensureAlways(startVertex && endVertex))
			{
				FVector2D edgeDelta = endVertex->Position - startVertex->Position;
				float edgeLength = edgeDelta.Size();
				if (edgeLength > Graph->Epsilon)
				{
					CachedEdgeDir = edgeDelta / edgeLength;
					CachedAngle = FRotator::ClampAxis(FMath::RadiansToDegrees(FMath::Atan2(CachedEdgeDir.Y, CachedEdgeDir.X)));
				}
				return true;
			}
		}

		return false;
	}
}
