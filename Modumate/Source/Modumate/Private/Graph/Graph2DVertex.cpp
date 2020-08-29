#include "Graph/Graph2DVertex.h"

#include "Graph/Graph2D.h"

namespace Modumate
{
	FGraph2DVertex::FGraph2DVertex(int32 InID, TWeakPtr<FGraph2D> InGraph, const FVector2D &InPos)
		: IGraph2DObject(InID, InGraph)
		, Position(InPos)
	{
		bValid = true;
	}

	void FGraph2DVertex::SetPosition(const FVector2D& NewPosition)
	{
		Position = NewPosition;
		Dirty(true);
	}

	bool FGraph2DVertex::AddEdge(FGraphSignedID EdgeID)
	{
		if (Edges.AddUnique(EdgeID) == INDEX_NONE)
		{
			return false;
		}

		Dirty(false);
		return true;
	}

	bool FGraph2DVertex::RemoveEdge(FGraphSignedID EdgeID)
	{
		int32 numRemoved = Edges.Remove(EdgeID);
		if (numRemoved == 0)
		{
			return false;
		}

		Dirty(false);
		return true;
	}

	bool FGraph2DVertex::GetNextEdge(FGraphSignedID CurEdgeID, FGraphSignedID &OutNextEdgeID, float &OutAngleDelta,
		const TSet<FGraphSignedID>* AllowedEdgeIDs, bool bForwards) const
	{
		OutNextEdgeID = MOD_ID_NONE;
		OutAngleDelta = 0.0f;

		FGraphSignedID curEdgeFromVertexID = -CurEdgeID;
		int32 curEdgeIdx = Edges.Find(curEdgeFromVertexID);
		if (curEdgeIdx == INDEX_NONE)
		{
			return false;
		}

		int32 indexDelta = bForwards ? 1 : -1;

		// Loop around the connected edges in the desired direction until we find an allowed one
		int32 numEdges = Edges.Num();
		for (int32 i = 1; i <= numEdges; ++i)
		{
			int32 nextEdgeIdx = (curEdgeIdx + numEdges + (i * indexDelta)) % numEdges;
			FGraphSignedID nextEdgeID = Edges[nextEdgeIdx];
			if ((AllowedEdgeIDs == nullptr) || AllowedEdgeIDs->Contains(nextEdgeID))
			{
				OutNextEdgeID = nextEdgeID;
				break;
			}
		}

		if (OutNextEdgeID == MOD_ID_NONE)
		{
			return false;
		}

		// If we're doubling back on the same edge, then consider it a full rotation
		if (curEdgeFromVertexID == OutNextEdgeID)
		{
			OutAngleDelta = 360.0f;
			return true;
		}

		auto graph = Graph.Pin();
		if (!graph.IsValid())
		{
			return false;
		}

		float curEdgeAngle = 0.0f, nextEdgeAngle = 0.0f;
		if (graph->GetEdgeAngle(curEdgeFromVertexID, curEdgeAngle) &&
			graph->GetEdgeAngle(OutNextEdgeID, nextEdgeAngle) &&
			ensureAlways(!FMath::IsNearlyEqual(curEdgeAngle, nextEdgeAngle)))
		{
			OutAngleDelta = nextEdgeAngle - curEdgeAngle;
			if (OutAngleDelta < 0.0f)
			{
				OutAngleDelta += 360.0f;
			}

			return true;
		}

		return false;
	}

	void FGraph2DVertex::Dirty(bool bConnected)
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

			for (int32 edgeID : Edges)
			{
				auto edge = graph->FindEdge(edgeID);
				if (edge == nullptr)
				{
					continue;
				}

				edge->Dirty(bContinueConnected);

				auto leftPoly = graph->FindPolygon(edge->LeftPolyID);
				if (leftPoly != nullptr)
				{
					leftPoly->Dirty(bContinueConnected);
				}

				auto rightPoly = graph->FindPolygon(edge->RightPolyID);
				if (rightPoly != nullptr)
				{
					rightPoly->Dirty(bContinueConnected);
				}
			}
		}
	}

	bool FGraph2DVertex::Clean()
	{
		if (!bDerivedDataDirty)
		{
			return false;
		}

		if (ensure(SortEdges()))
		{
			bDerivedDataDirty = false;
		}

		return true;
	}

	void FGraph2DVertex::GetVertexIDs(TArray<int32> &OutVertexIDs) const
	{
		OutVertexIDs = { ID };
	}

	bool FGraph2DVertex::SortEdges()
	{
		auto graph = Graph.Pin();
		if (!graph.IsValid())
		{
			return false;
		}

		if (graph->bDebugCheck)
		{
			for (FGraphSignedID edgeID : Edges)
			{
				FGraph2DEdge *edge = graph->FindEdge(edgeID);
				if (!ensureAlways(edge && edge->bValid && !edge->bDerivedDataDirty))
				{
					return false;
				}

				int32 connectedVertexID = (edgeID > 0) ? edge->StartVertexID : edge->EndVertexID;
				if (!ensureAlways(connectedVertexID == ID))
				{
					return false;
				}
			}
		}

		auto edgeSorter = [graph](const FGraphSignedID &edgeID1, const FGraphSignedID &edgeID2) {
			float angle1 = 0.0f, angle2 = 0.0f;
			if (ensureAlways(graph->GetEdgeAngle(edgeID1, angle1) && graph->GetEdgeAngle(edgeID2, angle2)))
			{
				return angle1 < angle2;
			}
			else
			{
				return false;
			}
		};

		Edges.Sort(edgeSorter);
		return true;
	}
}
