// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DEdge.h"

#include "Graph/Graph2D.h"

namespace Modumate
{
	FGraph2DEdge::FGraph2DEdge(int32 InID, FGraph2D* InGraph, int32 InStart, int32 InEnd)
		: IGraph2DObject(InID, InGraph)
		, LeftPolyID(0)
		, RightPolyID(0)
		, Angle(0.0f)
		, EdgeDir(ForceInitToZero)
	{
		SetVertices(InStart, InEnd);
	}

	void FGraph2DEdge::SetVertices(int32 InStart, int32 InEnd)
	{
		StartVertexID = InStart;
		EndVertexID = InEnd;
		bValid = CacheAngle();
	}

	bool FGraph2DEdge::CacheAngle()
	{
		Angle = 0.0f;
		EdgeDir = FVector2D::ZeroVector;

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
					EdgeDir = edgeDelta / edgeLength;
					Angle = FRotator::ClampAxis(FMath::RadiansToDegrees(FMath::Atan2(EdgeDir.Y, EdgeDir.X)));
				}
				return true;
			}
		}

		return false;
	}

	void FGraph2DEdge::Dirty(bool bConnected)
	{
		bDirty = true;

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

	void FGraph2DEdge::Clean()
	{
		bDirty = false;
	}
}
