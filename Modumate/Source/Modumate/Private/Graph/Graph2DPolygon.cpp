// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DPolygon.h"

#include "Graph/Graph2D.h"

#include "ModumateCore/ModumateFunctionLibrary.h"

namespace Modumate
{
	FGraph2DPolygon::FGraph2DPolygon(int32 InID, FGraph2D* InGraph, TArray<int32> &Vertices)
		: IGraph2DObject(InID, InGraph)
	{
		SetVertices(Vertices);
	}

	bool FGraph2DPolygon::IsInside(const FGraph2DPolygon &otherPoly) const
	{
		// Polygons cannot be contained inside exterior polygons
		if (!otherPoly.bInterior)
		{
			return false;
		}

		// If we've already established the other poly as a parent, return the cached result
		if (ParentID == otherPoly.ID)
		{
			return true;
		}

		// If this is a parent of the other polygon, the parent cannot be contained within the child
		if (otherPoly.ParentID == ID)
		{
			return false;
		}

		// Early out by checking the AABBs
		if (!otherPoly.AABB.IsInside(AABB))
		{
			return false;
		}

		for (const FVector2D &point : CachedPoints)
		{
			if (UModumateFunctionLibrary::PointInPoly2D(point, otherPoly.CachedPoints))
			{
				return true;
			}
		}

		return false;
	}

	void FGraph2DPolygon::SetParent(int32 inParentID)
	{
		if (ParentID != inParentID)
		{
			// remove self from old parent
			if (ParentID != 0)
			{
				FGraph2DPolygon *parentPoly = Graph->FindPolygon(ParentID);
				// parentPoly may be nullptr if the parent has been deleted
				if (parentPoly != nullptr)
				{
					int32 numRemoved = parentPoly->InteriorPolygons.Remove(ID);
					ensureAlways(numRemoved == 1);
				}
			}

			ParentID = inParentID;

			// add self to new parent
			if (ParentID != 0)
			{
				FGraph2DPolygon *parentPoly = Graph->FindPolygon(ParentID);
				if (ensureAlways(parentPoly && !parentPoly->InteriorPolygons.Contains(ID)))
				{
					parentPoly->InteriorPolygons.Add(ID);
				}
			}
		}
	}

	void FGraph2DPolygon::SetVertices(TArray<int32> &Vertices)
	{
		int32 numVertices = Vertices.Num();

		CachedPoints.Reset();
		VertexIDs.Reset();
		Edges.Reset();
		bHasDuplicateEdge = false;

		for (int32 vertexIdx = 0; vertexIdx < numVertices; vertexIdx++)
		{
			int32 vertexID = Vertices[vertexIdx];
			int32 nextVertexID = Vertices[(vertexIdx + 1) % numVertices];

			auto vertex = Graph->FindVertex(vertexID);
			if (!ensureAlways(vertex)) return;

			CachedPoints.Add(vertex->Position);
			VertexIDs.Add(vertexID);
			
			bool bOutForward;
			auto edge = Graph->FindEdgeByVertices(vertexID, nextVertexID, bOutForward);
			if (!ensureAlways(edge)) return;

			// updated edge's left/right poly to be this one
			(bOutForward) ? (edge->LeftPolyID = ID) : (edge->RightPolyID = ID);

			// detect "peninsula" in edge loop
			if (Edges.Num() > 0 && -edge->ID == Edges[Edges.Num() - 1])
			{	// TODO: this does not cover all cases of duplicate edges
				bHasDuplicateEdge = true;
			}

			Edges.Add(bOutForward ? edge->ID : -edge->ID);
		}

		AABB = FBox2D(CachedPoints);
	}

	void FGraph2DPolygon::Dirty(bool bConnected)
	{
		bDirty = true;

		if (bConnected)
		{
			bool bContinueConnected = false;
			for (int32 edgeID : Edges)
			{
				if (auto edge = Graph->FindEdge(edgeID))
				{
					edge->Dirty(bContinueConnected);

					// TODO: should polygons maintain a list of vertices instead of (or in addition to) positions?
					auto vertex = (edgeID < 0) ? Graph->FindVertex(edge->EndVertexID) : Graph->FindVertex(edge->StartVertexID);
					if (vertex != nullptr)
					{
						vertex->Dirty(bContinueConnected);
					}
				}
			}
		}
	}

	void FGraph2DPolygon::Clean()
	{
		bDirty = false;
	}
}
