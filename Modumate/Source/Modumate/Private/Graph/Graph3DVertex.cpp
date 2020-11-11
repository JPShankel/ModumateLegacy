#include "Graph/Graph3DVertex.h"
#include "Graph/Graph3D.h"

namespace Modumate {
	FGraph3DVertex::FGraph3DVertex(int32 InID, FGraph3D* InGraph, const FVector &InPos, const TSet<int32> &InGroupIDs)
		: IGraph3DObject(InID, InGraph, InGroupIDs)
		, Position(InPos)
	{
		bValid = true;
	}

	void FGraph3DVertex::AddEdge(FGraphSignedID EdgeID)
	{
		ConnectedEdgeIDs.AddUnique(EdgeID);
	}

	bool FGraph3DVertex::RemoveEdge(FGraphSignedID EdgeID)
	{
		int32 numRemoved = ConnectedEdgeIDs.Remove(EdgeID);
		return (numRemoved != 0);
	}

	void FGraph3DVertex::GetConnectedFaces(TSet<int32>& OutFaceIDs) const
	{
		for (int32 edgeID : ConnectedEdgeIDs)
		{
			auto edge = Graph->FindEdge(edgeID);
			for (auto faceConnection : edge->ConnectedFaces)
			{
				if (!faceConnection.bContained)
				{
					OutFaceIDs.Add(FMath::Abs(faceConnection.FaceID));
				}
			}
		}
	}

	void FGraph3DVertex::GetConnectedFacesAndEdges(TSet<int32>& OutFaceIDs, TSet<int32>& OutEdgeIDs) const
	{
		for (int32 edgeID : ConnectedEdgeIDs)
		{
			auto edge = Graph->FindEdge(edgeID);
			OutEdgeIDs.Add(FMath::Abs(edgeID));
			for (auto faceConnection : edge->ConnectedFaces)
			{
				if (!faceConnection.bContained)
				{
					OutFaceIDs.Add(FMath::Abs(faceConnection.FaceID));
				}
			}
		}
	}

	void FGraph3DVertex::Dirty(bool bConnected)
	{
		bDirty = true;

		if (bConnected)
		{
			bool bContinueConnected = false;
			
			for (int32 edgeID : ConnectedEdgeIDs)
			{
				// connected edges
				auto edge = Graph->FindEdge(edgeID);
				if (edge == nullptr)
				{
					continue;
				}

				edge->Dirty(bContinueConnected);

				// connected faces
				for (auto faceConnection : edge->ConnectedFaces)
				{
					auto face = Graph->FindFace(faceConnection.FaceID);
					if (face == nullptr)
					{
						continue;
					}
					face->Dirty(bContinueConnected);
				}
			}
		}
	}

	bool FGraph3DVertex::ValidateForTests() const
	{
		if (Graph == nullptr)
		{
			return false;
		}

		for (int32 edgeID : ConnectedEdgeIDs)
		{
			auto edge = Graph->FindEdge(edgeID);
			if (edge == nullptr)
			{
				return false;
			}
		}
		return true;
	}

	void FGraph3DVertex::GetVertexIDs(TArray<int32>& OutVertexIDs) const
	{
		OutVertexIDs = { ID };
	}
}
