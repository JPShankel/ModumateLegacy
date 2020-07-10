#include "Graph/Graph2DVertex.h"

#include "Graph/Graph2D.h"

namespace Modumate
{
	void FGraph2DVertex::AddEdge(FEdgeID EdgeID)
	{
		Edges.AddUnique(EdgeID);
		bDirty = true;
	}

	bool FGraph2DVertex::RemoveEdge(FEdgeID EdgeID)
	{
		int32 numRemoved = Edges.Remove(EdgeID);
		if (numRemoved != 0)
		{
			bDirty = true;
			return true;
		}

		return false;
	}

	void FGraph2DVertex::SortEdges()
	{
		if (bDirty)
		{
			if (Graph->bDebugCheck)
			{
				for (FEdgeID edgeID : Edges)
				{
					FGraph2DEdge *edge = Graph->FindEdge(edgeID);
					if (ensureAlways(edge && edge->bValid))
					{
						int32 connectedVertexID = (edgeID > 0) ? edge->StartVertexID : edge->EndVertexID;
						ensureAlways(connectedVertexID == ID);
					}
				}
			}

			auto edgeSorter = [this](const FEdgeID &edgeID1, const FEdgeID &edgeID2) {
				float angle1 = 0.0f, angle2 = 0.0f;
				if (ensureAlways(Graph->GetEdgeAngle(edgeID1, angle1) && Graph->GetEdgeAngle(edgeID2, angle2)))
				{
					return angle1 < angle2;
				}
				else
				{
					return false;
				}
			};

			Edges.Sort(edgeSorter);
			bDirty = false;
		}
	}

	bool FGraph2DVertex::GetNextEdge(FEdgeID curEdgeID, FEdgeID &outNextEdgeID, float &outAngleDelta, int32 indexDelta) const
	{
		outNextEdgeID = 0;
		outAngleDelta = 0.0f;

		FEdgeID curEdgeFromVertexID = -curEdgeID;
		int32 curEdgeIdx = Edges.Find(curEdgeFromVertexID);
		if (curEdgeIdx == INDEX_NONE)
		{
			return false;
		}

		if (indexDelta == 0)
		{
			return false;
		}

		int32 numEdges = Edges.Num();
		int32 nextEdgeIdx = (curEdgeIdx + indexDelta) % numEdges;
		outNextEdgeID = Edges[nextEdgeIdx];
		if (outNextEdgeID == 0)
		{
			return false;
		}

		// If we're doubling back on the same edge, then consider it a full rotation
		if (curEdgeFromVertexID == outNextEdgeID)
		{
			outAngleDelta = 360.0f;
			return true;
		}

		float curEdgeAngle = 0.0f, nextEdgeAngle = 0.0f;
		if (Graph->GetEdgeAngle(curEdgeFromVertexID, curEdgeAngle) &&
			Graph->GetEdgeAngle(outNextEdgeID, nextEdgeAngle) &&
			ensureAlways(!FMath::IsNearlyEqual(curEdgeAngle, nextEdgeAngle)))
		{
			outAngleDelta = nextEdgeAngle - curEdgeAngle;
			if (outAngleDelta < 0.0f)
			{
				outAngleDelta += 360.0f;
			}

			return true;
		}

		return false;
	}

	void FGraph2DVertex::Dirty(bool bConnected)
	{
		bDirty = true;

		if (bConnected)
		{
			bool bContinueConnected = false;

			for (int32 edgeID : Edges)
			{
				auto edge = Graph->FindEdge(edgeID);
				if (edge == nullptr)
				{
					continue;
				}

				edge->Dirty(bContinueConnected);

				auto leftPoly = Graph->FindPolygon(edge->LeftPolyID);
				if (leftPoly != nullptr)
				{
					leftPoly->Dirty(bContinueConnected);
				}

				auto rightPoly = Graph->FindPolygon(edge->RightPolyID);
				if (rightPoly != nullptr)
				{
					rightPoly->Dirty(bContinueConnected);
				}
			}
		}
	}

	void FGraph2DVertex::Clean()
	{
		bDirty = false;
	}
}
