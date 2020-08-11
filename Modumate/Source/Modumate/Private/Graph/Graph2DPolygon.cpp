// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DPolygon.h"

#include "Graph/Graph2D.h"

#include "ModumateCore/ModumateFunctionLibrary.h"

namespace Modumate
{
	FGraph2DPolygon::FGraph2DPolygon(int32 InID, FGraph2D* InGraph, TArray<int32> &Vertices, bool bInInterior)
		: IGraph2DObject(InID, InGraph)
		, bInterior(bInInterior)
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

	void FGraph2DPolygon::SetVertices(const TArray<int32> &Vertices)
	{
		int32 numVertices = Vertices.Num();

		CachedPoints.Reset();
		VertexIDs.Reset();
		Edges.Reset();
		TempVertexSet.Reset();
		bHasDuplicateVertex = false;

		for (int32 vertexIdx = 0; vertexIdx < numVertices; vertexIdx++)
		{
			int32 vertexID = Vertices[vertexIdx];
			int32 nextVertexID = Vertices[(vertexIdx + 1) % numVertices];

			auto vertex = Graph->FindVertex(vertexID);
			if (!ensureAlways(vertex))
			{
				return;
			}

			CachedPoints.Add(vertex->Position);
			VertexIDs.Add(vertexID);
			TempVertexSet.Add(vertexID);

			bool bCurEdgeForward;
			auto edge = Graph->FindEdgeByVertices(vertexID, nextVertexID, bCurEdgeForward);
			if (!ensureAlways(edge))
			{
				return;
			}

			// updated edge's left/right poly to be this one
			if (bCurEdgeForward)
			{
				edge->LeftPolyID = ID;
			}
			else
			{
				edge->RightPolyID = ID;
			}

			Edges.Add(bCurEdgeForward ? edge->ID : -edge->ID);
		}

		bHasDuplicateVertex = (VertexIDs.Num() != TempVertexSet.Num());

		AABB = FBox2D(CachedPoints);
		CalculatePerimeter();
	}

	void FGraph2DPolygon::CalculatePerimeter()
	{
		CachedPerimeterVertexIDs.Reset();
		CachedPerimeterEdgeIDs.Reset();
		CachedPerimeterPoints.Reset();

		// Only interior non-simple polygonal traversals are guaranteed to have simple polygonal perimeter traversals
		int32 numEdges = Edges.Num();
		if ((numEdges == 0) || !bInterior)
		{
			return;
		}

#define SUPER_DEBUG 0
		// TODO: remove very excessive debug testing and cleaning

		if (Graph->bDebugCheck)
		{
#if SUPER_DEBUG
			TArray<FGraphSignedID> testEdgeIDs;
			TArray<int32> testVertexIDs;
			bool bTestInterior;
			TempVisitedEdges.Empty(numEdges);
			bool bValidPolygon = Graph->TraverseEdges(Edges[0], testEdgeIDs, testVertexIDs, bTestInterior, TempVisitedEdges);
			ensureAlways((testEdgeIDs == Edges) && (testVertexIDs == VertexIDs) && (bTestInterior == bInterior));
#endif

			for (int32 vertexID : VertexIDs)
			{
				auto vertex = Graph->FindVertex(vertexID);
				TArray<FGraphSignedID> oldEdges = vertex->Edges;
				vertex->Dirty(false);
				vertex->SortEdges();
#if SUPER_DEBUG
				ensureAlways(vertex->Edges == oldEdges);
#endif
			}
		}

#undef SUPER_DEBUG

		TempAllowedPerimeterEdges.Empty(numEdges);
		for (FGraphSignedID edgeID : Edges)
		{
			TempAllowedPerimeterEdges.Add(-edgeID);
		}

		// Try to traverse an external simple polygonal perimeter by going in the opposite direction of one of the edges in the polygon.
		// We expect the traversal to fail if we start on an edge that is in the original traversal twice (a "peninsula" edge),
		// so we may have to try multiple.
		bool bValidPerimeter = false;
		for (int32 edgeIdx = 0; (edgeIdx < numEdges) && !bValidPerimeter; ++edgeIdx)
		{
			FGraphSignedID startingEdgeID = -Edges[edgeIdx];

			// Don't try to traverse perimeters from edges that are duplicated in the original traversal
			if (TempAllowedPerimeterEdges.Contains(-startingEdgeID))
			{
				continue;
			}

			// This may still fail if we start traversing from an edge on an "island" contained within the polygon,
			// since it only appears once within the original traversal's edge list, so we didn't already skip it.
			bool bPerimeterInterior;
			TempVisitedEdges.Empty(numEdges);
			bValidPerimeter = Graph->TraverseEdges(startingEdgeID, CachedPerimeterEdgeIDs, CachedPerimeterVertexIDs, bPerimeterInterior, TempVisitedEdges, false, &TempAllowedPerimeterEdges);
			bValidPerimeter = bValidPerimeter && !bPerimeterInterior;

			if (bValidPerimeter)
			{
				// TODO: remove debug check if it turns out it's impossible to reach
				if (Graph->bDebugCheck)
				{
					TempVertexSet.Empty(CachedPerimeterVertexIDs.Num());
					TempVertexSet.Append(CachedPerimeterVertexIDs);
					if (!ensureAlways(CachedPerimeterVertexIDs.Num() == TempVertexSet.Num()))
					{
						bValidPerimeter = false;
					}
				}
			}
		}

		// TODO: identify and fix cases where graphs with polygons that made it this far were unable to come up with a valid perimeter.
		// Regardless, the existence of CachedPerimeter* members reflects whether the perimeter exists.
		if (ensureAlways(bValidPerimeter))
		{
			for (int32 perimeterVertexID : CachedPerimeterVertexIDs)
			{
				CachedPerimeterPoints.Add(Graph->FindVertex(perimeterVertexID)->Position);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Graph2DPolygon #%d failed to calculate a valid perimeter!"), ID);
			CachedPerimeterVertexIDs.Reset();
			CachedPerimeterEdgeIDs.Reset();
		}
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
