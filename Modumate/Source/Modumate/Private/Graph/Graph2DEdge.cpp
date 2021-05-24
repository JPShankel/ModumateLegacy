// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DEdge.h"

#include "Graph/Graph2D.h"

FGraph2DEdge::FGraph2DEdge(int32 InID, TWeakPtr<FGraph2D> InGraph, int32 InStart, int32 InEnd)
	: IGraph2DObject(InID, InGraph)
{
	SetVertices(InStart, InEnd);
}

void FGraph2DEdge::SetVertices(int32 InStart, int32 InEnd)
{
	auto graph = Graph.Pin();
	if (!ensure(graph.IsValid()))
	{
		return;
	}

	if (StartVertexID != MOD_ID_NONE)
	{
		auto startVertex = graph->FindVertex(StartVertexID);
		ensureAlways(startVertex && startVertex->RemoveEdge(ID));
	}

	if (EndVertexID != MOD_ID_NONE)
	{
		auto endVertex = graph->FindVertex(EndVertexID);
		ensureAlways(endVertex && endVertex->RemoveEdge(-ID));
	}

	StartVertexID = InStart;
	EndVertexID = InEnd;
	bValid = (StartVertexID != MOD_ID_NONE) && (EndVertexID != MOD_ID_NONE);
	Dirty(false);

	if (StartVertexID != MOD_ID_NONE)
	{
		auto startVertex = graph->FindVertex(StartVertexID);
		ensureAlways(startVertex && startVertex->AddEdge(ID));
	}

	if (EndVertexID != MOD_ID_NONE)
	{
		auto endVertex = graph->FindVertex(EndVertexID);
		ensureAlways(endVertex && endVertex->AddEdge(-ID));
	}
}

bool FGraph2DEdge::GetAdjacentInteriorPolygons(const FGraph2DPolygon*& OutPolyLeft, const FGraph2DPolygon*& OutPolyRight) const
{
	OutPolyLeft = OutPolyRight = nullptr;

	auto graph = Graph.Pin();
	if (!ensure(graph.IsValid()) || !bValid || bDerivedDataDirty)
	{
		return false;
	}

	auto polyLeft = graph->FindPolygon(LeftPolyID);
	auto polyRight = graph->FindPolygon(RightPolyID);
	for (int32 sideIdx = 0; sideIdx < 2; ++sideIdx)
	{
		auto adjacentPoly = (sideIdx == 0) ? polyLeft : polyRight;
		auto& outPoly = (sideIdx == 0) ? OutPolyLeft : OutPolyRight;

		if (adjacentPoly == nullptr)
		{
			continue;
		}

		if (adjacentPoly->bInterior)
		{
			outPoly = adjacentPoly;
		}
		else if (adjacentPoly->ContainingPolyID != MOD_ID_NONE)
		{
			auto adjacentPolyContainer = graph->FindPolygon(adjacentPoly->ContainingPolyID);
			if (adjacentPolyContainer && adjacentPolyContainer->bInterior)
			{
				outPoly = adjacentPolyContainer;
			}
		}
	}

	return true;
}

void FGraph2DEdge::Dirty(bool bConnected)
{
	IGraph2DObject::Dirty(bConnected);
	bPolygonDirty = true;

	if (bConnected)
	{
		auto graph = Graph.Pin();
		if (!ensure(graph.IsValid()))
		{
			return;
		}

		bool bContinueConnected = false;

		auto startVertex = graph->FindVertex(StartVertexID);
		if (startVertex != nullptr)
		{
			startVertex->Dirty(bContinueConnected);
		}

		auto endVertex = graph->FindVertex(EndVertexID);
		if (endVertex != nullptr)
		{
			endVertex->Dirty(bContinueConnected);
		}

		auto leftPoly = graph->FindPolygon(LeftPolyID);
		if (leftPoly != nullptr)
		{
			leftPoly->Dirty(bContinueConnected);
		}

		auto rightPoly = graph->FindPolygon(RightPolyID);
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
	auto graph = Graph.Pin();

	if (ensureAlways(graph.IsValid()) && (StartVertexID != 0) && (EndVertexID != 0))
	{
		FGraph2DVertex *startVertex = graph->FindVertex(StartVertexID);
		FGraph2DVertex *endVertex = graph->FindVertex(EndVertexID);
		if (ensureAlways(startVertex && endVertex))
		{
			FVector2D edgeDelta = endVertex->Position - startVertex->Position;
			float edgeLength = edgeDelta.Size();
			if (edgeLength > graph->Epsilon)
			{
				CachedEdgeDir = edgeDelta / edgeLength;
				CachedAngle = FRotator::ClampAxis(FMath::RadiansToDegrees(FMath::Atan2(CachedEdgeDir.Y, CachedEdgeDir.X)));
			}
			return true;
		}
	}

	return false;
}

