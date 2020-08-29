// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DPolygon.h"

#include "Graph/Graph2D.h"

#include "ModumateCore/ModumateFunctionLibrary.h"

namespace Modumate
{
	TSet<int32> FGraph2DPolygon::TempVertexSet;
	TSet<FGraphSignedID> FGraph2DPolygon::TempVisitedEdges;

	FGraph2DPolygon::FGraph2DPolygon()
		: IGraph2DObject(MOD_ID_NONE, nullptr)
	{ }

	FGraph2DPolygon::FGraph2DPolygon(int32 InID, TWeakPtr<FGraph2D> InGraph)
		: IGraph2DObject(InID, InGraph)
	{ }

	FGraph2DPolygon::FGraph2DPolygon(int32 InID, TWeakPtr<FGraph2D> InGraph, const TArray<int32>& InVertices)
		: IGraph2DObject(InID, InGraph)
	{
		SetVertices(InVertices);
	}

	bool FGraph2DPolygon::IsInside(int32 OtherPolyID) const
	{
		auto graph = Graph.Pin();
		const FGraph2DPolygon* otherPoly = graph ? graph->FindPolygon(OtherPolyID) : nullptr;

		if (!ensureAlways(!bDerivedDataDirty && otherPoly && !otherPoly->bDerivedDataDirty))
		{
			return false;
		}

		// Exterior polygons do not participate in containment
		if (!bInterior || !otherPoly->bInterior)
		{
			return false;
		}

		// If we've already established the other poly as a parent, return the cached result
		if (ContainingPolyID == otherPoly->ID)
		{
			return true;
		}

		// If this is a parent of the other polygon, the parent cannot be contained within the child
		if (otherPoly->ContainingPolyID == ID)
		{
			return false;
		}

		// Early out by checking the AABBs
		if (!otherPoly->AABB.ExpandBy(graph->Epsilon).IsInside(AABB))
		{
			return false;
		}

		// Now, check the actual polygon perimeters for geometric containment, allowing partial containment with a shared vertex
		bool bFullyContained, bPartiallyContained;
		bool bValidContainment = UModumateGeometryStatics::GetPolygonContainment(otherPoly->CachedPerimeterPoints, CachedPerimeterPoints, bFullyContained, bPartiallyContained);
		return bValidContainment && (bFullyContained || bPartiallyContained);
	}

	void FGraph2DPolygon::SetContainingPoly(int32 NewContainingPolyID)
	{
		auto graph = Graph.Pin();
		if (!ensure(graph.IsValid()))
		{
			return;
		}

		if (ContainingPolyID != NewContainingPolyID)
		{
			// remove self from old parent
			if (ContainingPolyID != MOD_ID_NONE)
			{
				FGraph2DPolygon *parentPoly = graph->FindPolygon(ContainingPolyID);
				// parentPoly may be nullptr if the parent has been deleted
				if (parentPoly != nullptr)
				{
					int32 numRemoved = parentPoly->ContainedPolyIDs.Remove(ID);
					ensureAlways(numRemoved == 1);
				}
			}

			ContainingPolyID = NewContainingPolyID;

			// add self to new parent
			if (ContainingPolyID != MOD_ID_NONE)
			{
				FGraph2DPolygon *parentPoly = graph->FindPolygon(ContainingPolyID);
				if (ensureAlways(parentPoly && !parentPoly->ContainedPolyIDs.Contains(ID)))
				{
					parentPoly->ContainedPolyIDs.Add(ID);
				}
			}
		}
		else if (ContainingPolyID != MOD_ID_NONE)
		{
			FGraph2DPolygon *parentPoly = graph->FindPolygon(ContainingPolyID);
			ensureAlways(parentPoly && parentPoly->ContainedPolyIDs.Contains(ID));
		}
	}

	void FGraph2DPolygon::SetVertices(const TArray<int32> &InVertexIDs)
	{
		int32 numVertices = InVertexIDs.Num();

		bValid = false;
		VertexIDs.Reset(numVertices);
		Edges.Reset(numVertices);
		bHasDuplicateVertex = false;
		AABB.Init();
		CachedPoints.Reset(numVertices);
		CachedPerimeterVertexIDs.Reset();
		CachedPerimeterEdgeIDs.Reset();
		CachedPerimeterPoints.Reset();
		TempVertexSet.Reset();

		ensureAlways(&InVertexIDs != &VertexIDs);
		auto graph = Graph.Pin();
		if (!ensure(graph.IsValid()))
		{
			return;
		}
		
		Dirty(false);

		for (int32 vertexIdx = 0; vertexIdx < numVertices; vertexIdx++)
		{
			int32 vertexID = InVertexIDs[vertexIdx];
			int32 nextVertexID = InVertexIDs[(vertexIdx + 1) % numVertices];

			auto vertex = graph->FindVertex(vertexID);
			if (!ensureAlways(vertex && vertex->bValid && !vertex->bDerivedDataDirty))
			{
				return;
			}

			CachedPoints.Add(vertex->Position);
			VertexIDs.Add(vertexID);
			TempVertexSet.Add(vertexID);
			AABB += vertex->Position;

			bool bCurEdgeForward;
			auto edge = graph->FindEdgeByVertices(vertexID, nextVertexID, bCurEdgeForward);
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

		// Extremely basic validity check; traversal and perimeter calculation will be more telling for polygons that can use it
		bValid = numVertices > 1;
		bHasDuplicateVertex = (numVertices != TempVertexSet.Num());
	}

	bool FGraph2DPolygon::CalculateTraversal(bool& bOutCalculatedInterior)
	{
		int32 numEdges = Edges.Num();
		auto graph = Graph.Pin();
		if ((numEdges == 0) || !ensure(graph.IsValid()))
		{
			return false;
		}

		TArray<FGraphSignedID> testEdgeIDs;
		TArray<int32> testVertexIDs;
		TempVisitedEdges.Empty(numEdges);
		bool bValidPolygon = graph->TraverseEdges(Edges[0], testEdgeIDs, testVertexIDs, bOutCalculatedInterior, TempVisitedEdges);
		return bValidPolygon && (testEdgeIDs == Edges) && (testVertexIDs == VertexIDs);
	}

	bool FGraph2DPolygon::CalculatePerimeter()
	{
		CachedPerimeterVertexIDs.Reset();
		CachedPerimeterEdgeIDs.Reset();
		CachedPerimeterPoints.Reset();

		auto graph = Graph.Pin();
		if (!ensure(graph.IsValid()))
		{
			return false;
		}

		// Only interior non-simple polygonal traversals are guaranteed to have simple polygonal perimeter traversals
		
		if (!bValid || !bInterior)
		{
			return true;
		}

		int32 numEdges = Edges.Num();

		static TSet<FGraphSignedID> tempAllowedPerimeterEdges;
		tempAllowedPerimeterEdges.Empty(numEdges);
		for (FGraphSignedID edgeID : Edges)
		{
			tempAllowedPerimeterEdges.Add(-edgeID);
		}

		// Try to traverse an external simple polygonal perimeter by going in the opposite direction of one of the edges in the polygon.
		// We expect the traversal to fail if we start on an edge that is in the original traversal twice (a "peninsula" edge),
		// so we may have to try multiple.
		bool bValidPerimeter = false;
		for (int32 edgeIdx = 0; (edgeIdx < numEdges) && !bValidPerimeter; ++edgeIdx)
		{
			FGraphSignedID startingEdgeID = -Edges[edgeIdx];

			// Don't try to traverse perimeters from edges that are duplicated in the original traversal
			if (tempAllowedPerimeterEdges.Contains(-startingEdgeID))
			{
				continue;
			}

			// This may still fail if we start traversing from an edge on an "island" contained within the polygon,
			// since it only appears once within the original traversal's edge list, so we didn't already skip it.
			bool bPerimeterInterior;
			TempVisitedEdges.Empty(numEdges);
			bValidPerimeter = graph->TraverseEdges(startingEdgeID, CachedPerimeterEdgeIDs, CachedPerimeterVertexIDs, bPerimeterInterior, TempVisitedEdges, false, &tempAllowedPerimeterEdges);
			bValidPerimeter = bValidPerimeter && !bPerimeterInterior;

			if (bValidPerimeter)
			{
				// TODO: remove debug check if it turns out it's impossible to reach
				if (graph->bDebugCheck)
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
				CachedPerimeterPoints.Add(graph->FindVertex(perimeterVertexID)->Position);
			}

			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Graph2DPolygon #%d failed to calculate a valid perimeter!"), ID);
			CachedPerimeterVertexIDs.Reset();
			CachedPerimeterEdgeIDs.Reset();
			return false;
		}
	}

	void FGraph2DPolygon::Dirty(bool bConnected)
	{
		IGraph2DObject::Dirty(bConnected);

		if (bConnected)
		{
			auto graph = Graph.Pin();
			if (!ensure(graph.IsValid()))
			{
				return;
			}

			bool bContinueConnected = false;
			for (FGraphSignedID edgeID : Edges)
			{
				if (auto edge = graph->FindEdge(edgeID))
				{
					edge->Dirty(bContinueConnected);
				}
			}

			for (int32 vertexID : VertexIDs)
			{
				if (auto vertex = graph->FindVertex(vertexID))
				{
					vertex->Dirty(bContinueConnected);
				}
			}
		}
	}

	bool FGraph2DPolygon::Clean()
	{
		if (!bDerivedDataDirty)
		{
			return false;
		}

		static TArray<int32> tempVertexIDs;
		tempVertexIDs = VertexIDs;
		SetVertices(tempVertexIDs);

		if (ensure(CalculateTraversal(bInterior) && CalculatePerimeter()))
		{
			bDerivedDataDirty = false;
		}

		return true;
	}

	void FGraph2DPolygon::GetVertexIDs(TArray<int32> &OutVertexIDs) const
	{
		OutVertexIDs = VertexIDs;
	}
}
