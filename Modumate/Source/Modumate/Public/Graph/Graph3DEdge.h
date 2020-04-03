#include "ModumateGraph3DTypes.h"

#pragma once

namespace Modumate 
{
	struct FEdgeFaceConnection
	{
		FSignedID FaceID;		// The ID of the face, with sign indicating whether this edge is forward in the face's edge list
		FVector EdgeFaceDir;	// The internal normal of the edge to the face
		float FaceAngle;		// The angle of the face dir with respect to the edge's reference normal

		FEdgeFaceConnection(FSignedID InFaceID, FVector InEdgeFaceDir, float InFaceAngle)
			: FaceID(InFaceID)
			, EdgeFaceDir(InEdgeFaceDir)
			, FaceAngle(InFaceAngle)
		{ }
	};

	class FGraph3DFace;

	class FGraph3DEdge : public IGraph3DObject
	{
	public:
		int32 StartVertexID = MOD_ID_NONE;				// The ID of the vertex that the start of this edge is connected to
		int32 EndVertexID = MOD_ID_NONE;				// The ID of the vertex that the end of this edge is connected to
		TArray<FEdgeFaceConnection> ConnectedFaces;		// The list of faces connected to this edge, sorted in order from the reference normal

		FVector CachedDir = FVector::ZeroVector;		// The cached direction of the edge, from its start vertex to end vertex
		FVector CachedRefNorm = FVector::ZeroVector;	// The cached normal, an arbitrary reference vector that is normal to the direction, used for sorting angles
		FVector CachedMidpoint = FVector::ZeroVector;	// The cached midpoint between the start and end vertices.

		FGraph3DEdge(int32 InID, FGraph3D* InGraph, int32 InStart, int32 InEnd);

		void SetVertices(int32 InStart, int32 InEnd);
		bool AddFace(FSignedID FaceID, const FVector &EdgeFaceDir);
		bool RemoveFace(FSignedID FaceID, bool bRequireSameSign);
		void SortFaces();
		bool GetNextFace(FSignedID CurFaceID, FSignedID &OutNextFaceID, float &OutAngleDelta, int32 &OutNextFaceIndex) const;

		void GetAdjacentEdgeIDs(TSet<int32>& OutAdjEdgeIDs) const;

		bool IntersectsFace(const FGraph3DFace *OtherFace, FVector &IntersectingEdgeOrigin, FVector &IntersectingEdgeDir, TArray<FEdgeIntersection> &DestIntersections, bool &bOutOnFaceEdge) const;

		virtual void Dirty(bool bConnected = true) override;
		virtual bool ValidateForTests() const override;
	};
}
