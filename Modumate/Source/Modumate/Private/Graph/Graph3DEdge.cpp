#include "Graph3DEdge.h"
#include "Graph3D.h"
#include "ModumateGeometryStatics.h"

namespace Modumate
{
	FGraph3DEdge::FGraph3DEdge(int32 InID, FGraph3D* InGraph, int32 InStart, int32 InEnd, const TSet<int32> &InGroupIDs)
		: IGraph3DObject(InID, InGraph, InGroupIDs)
		, CachedDir(ForceInitToZero)
		, CachedRefNorm(ForceInitToZero)
		, CachedMidpoint(ForceInitToZero)
	{
		SetVertices(InStart, InEnd);
	}

	void FGraph3DEdge::SetVertices(int32 InStart, int32 InEnd)
	{
		StartVertexID = InStart;
		EndVertexID = InEnd;

		bValid = false;
		if (!ensureAlways(Graph) && (StartVertexID != 0) && (EndVertexID != 0))
		{
			// vertex IDs are invalid
			return;
		}

		FGraph3DVertex *startVertex = Graph->FindVertex(StartVertexID);
		FGraph3DVertex *endVertex = Graph->FindVertex(EndVertexID);
		if (!ensureAlways(startVertex && endVertex))
		{
			// Vertices are invalid
			return;
		}

		FVector edgeDelta = endVertex->Position - startVertex->Position;
		float edgeLength = edgeDelta.Size();
		if (!ensureAlways(edgeLength > Graph->Epsilon))
		{
			// edge positions are invalid
			return;
		}

		startVertex->AddEdge(ID);
		endVertex->AddEdge(-ID);

		CachedDir = edgeDelta / edgeLength;

		// Choose a reference normal vector
		if (FVector::Parallel(CachedDir, FVector::ForwardVector))
		{
			CachedRefNorm = FVector::RightVector;
		}
		else
		{
			CachedRefNorm = (CachedDir ^ FVector::ForwardVector).GetSafeNormal();
		}

		CachedMidpoint = 0.5f * (startVertex->Position + endVertex->Position);

		bValid = true;
		bDirty = (ConnectedFaces.Num() > 0);
	}

	bool FGraph3DEdge::AddFace(FSignedID FaceID, const FVector &EdgeFaceDir)
	{
		for (FEdgeFaceConnection &connectedFace : ConnectedFaces)
		{
			if (FMath::Abs(connectedFace.FaceID) == FMath::Abs(FaceID))
			{
				return false;
			}
		}

		if (!ensureAlways(FVector::Orthogonal(CachedDir, EdgeFaceDir)))
		{
			return false;
		}

		float absAngle = FMath::RadiansToDegrees(FMath::Acos(EdgeFaceDir | CachedRefNorm));
		float angleCross = (EdgeFaceDir ^ CachedRefNorm) | CachedDir;
		float faceAngle = (angleCross > 0.0f) ? absAngle : (360.0f - absAngle);

		ConnectedFaces.Add(FEdgeFaceConnection(FaceID, EdgeFaceDir, faceAngle));
		bDirty = true;

		return true;
	}

	bool FGraph3DEdge::RemoveFace(FSignedID FaceID, bool bRequireSameSign)
	{
		if (!bRequireSameSign)
		{
			FaceID = FMath::Abs(FaceID);
		}

		for (int32 i = ConnectedFaces.Num() - 1; i >= 0; --i)
		{
			int32 connectedFaceID = ConnectedFaces[i].FaceID;
			if (!bRequireSameSign)
			{
				connectedFaceID = FMath::Abs(connectedFaceID);
			}

			if (connectedFaceID == FaceID)
			{
				ConnectedFaces.RemoveAt(i);
				bDirty = true;

				return true;
			}
		}

		return false;
	}

	void FGraph3DEdge::SortFaces()
	{
		if (Graph->bDebugCheck)
		{
			for (FEdgeFaceConnection &connectedFace : ConnectedFaces)
			{
				FGraph3DFace *face = Graph->FindFace(connectedFace.FaceID);
				if (ensureAlways(face))
				{
					bool bFaceContainsThisEdge = false;
					for (FSignedID faceEdgeID : face->EdgeIDs)
					{
						if (FMath::Abs(faceEdgeID) == ID)
						{
							bFaceContainsThisEdge = true;
							break;
						}
					}

					ensureAlways(bFaceContainsThisEdge);
				}
			}
		}

		auto faceSorter = [this](const FEdgeFaceConnection &Face1, const FEdgeFaceConnection &Face2) {
			return Face1.FaceAngle < Face2.FaceAngle;
		};

		ConnectedFaces.Sort(faceSorter);
		bDirty = false;
	}

	bool FGraph3DEdge::GetNextFace(FSignedID CurFaceID, FSignedID &OutNextFaceID, float &OutAngleDelta, int32 &OutNextFaceIndex) const
	{
		OutNextFaceID = 0;
		OutAngleDelta = 0.0f;
		OutNextFaceIndex = INDEX_NONE;

		// Find the face connection data for the face from which we are trying to traverse
		int32 numConnectedFaces = ConnectedFaces.Num();
		int32 curFaceIdx = INDEX_NONE;
		for (int32 i = 0; i < numConnectedFaces; ++i)
		{
			const FEdgeFaceConnection &connectedFace = ConnectedFaces[i];
			if (FMath::Abs(connectedFace.FaceID) == FMath::Abs(CurFaceID))
			{
				curFaceIdx = i;
				break;
			}
		}

		if (curFaceIdx == INDEX_NONE)
		{
			return false;
		}

		const FEdgeFaceConnection &curFaceConnection = ConnectedFaces[curFaceIdx];
		FGraph3DFace *curFace = Graph->FindFace(CurFaceID);
		if (!ensureAlways(curFace))
		{
			return false;
		}

		// Determine whether this face's normal actually face's CW, or CCW, relative to this edge's rotation reference
		bool bCurFacePointsCW = ((curFaceConnection.EdgeFaceDir ^ curFace->CachedPlane) | CachedDir) < 0.0f;

		// Determine whether to traverse in the face's normal direction, or the opposite direction
		bool bTraverseCW = bCurFacePointsCW == (CurFaceID > 0);

		int32 faceDelta = bTraverseCW ? 1 : -1;
		OutNextFaceIndex = (curFaceIdx + faceDelta + numConnectedFaces) % numConnectedFaces;

		// If we're doubling back on the same face, then consider it a full rotation
		if (curFaceIdx == OutNextFaceIndex)
		{
			OutNextFaceID = -CurFaceID;
			OutAngleDelta = 360.0f;
			return true;
		}

		const FEdgeFaceConnection &nextFaceConnection = ConnectedFaces[OutNextFaceIndex];
		FGraph3DFace *nextFace = Graph->FindFace(nextFaceConnection.FaceID);
		if (!ensureAlways(nextFace))
		{
			return false;
		}

		bool bNextFacePointsCW = ((nextFaceConnection.EdgeFaceDir ^ nextFace->CachedPlane) | CachedDir) < 0.0f;
		OutNextFaceID = nextFace->ID * ((bNextFacePointsCW == bTraverseCW) ? -1 : 1);

		OutAngleDelta = nextFaceConnection.FaceAngle - curFaceConnection.FaceAngle;
		if (OutAngleDelta < 0.0f)
		{
			OutAngleDelta += 360.0f;
		}

		return true;
	}

	void FGraph3DEdge::GetAdjacentEdgeIDs(TSet<int32>& OutAdjEdgeIDs) const
	{
		OutAdjEdgeIDs.Reset();

		auto startVertex = Graph->FindVertex(StartVertexID);
		auto endVertex = Graph->FindVertex(EndVertexID);

		if (startVertex == nullptr || endVertex == nullptr)
		{
			return;
		}

		TSet<int32> connectedFaceIDs;
		startVertex->GetConnectedFacesAndEdges(connectedFaceIDs, OutAdjEdgeIDs);
		endVertex->GetConnectedFacesAndEdges(connectedFaceIDs, OutAdjEdgeIDs);
	}

	void FGraph3DEdge::Dirty(bool bConnected)
	{
		bDirty = true;

		if (bConnected)
		{
			bool bContinueConnected = false;

			// connected vertices
			auto startVertex = Graph->FindVertex(StartVertexID);
			auto endVertex = Graph->FindVertex(EndVertexID);

			if (startVertex == nullptr || endVertex == nullptr)
			{
				return;
			}

			startVertex->Dirty(bContinueConnected);
			startVertex->Dirty(bContinueConnected);

			// connected faces
			for (auto& connection : ConnectedFaces)
			{
				auto face = Graph->FindFace(connection.FaceID);
				if (face == nullptr)
				{
					continue;
				}

				face->Dirty(bContinueConnected);
			}
		}
	}

	bool FGraph3DEdge::ValidateForTests() const
	{
		if (Graph == nullptr)
		{
			return false;
		}

		auto startVertex = Graph->FindVertex(StartVertexID);
		auto endVertex = Graph->FindVertex(EndVertexID);

		if (startVertex == nullptr || endVertex == nullptr)
		{
			return false;
		}

		for (auto& connection : ConnectedFaces)
		{
			auto face = Graph->FindFace(connection.FaceID);
			if (face == nullptr)
			{
				return false;
			}
		}

		return true;
	}
}
