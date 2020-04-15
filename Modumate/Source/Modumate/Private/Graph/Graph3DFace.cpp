#include "Graph3DFace.h"
#include "Graph3D.h"
#include "Graph3DDelta.h"

#include "ModumateFunctionLibrary.h"
#include "ModumateGeometryStatics.h"


namespace Modumate 
{
	FGraph3DFace::FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<int32> &InVertexIDs)
		: IGraph3DObject(InID, InGraph)
		, CachedPlane(ForceInitToZero)
	{
		UpdatePlane(InVertexIDs);
		UpdateVerticesAndEdges(InVertexIDs, false);
	}

	FGraph3DFace::FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<FVector> &VertexPositions)
		: IGraph3DObject(InID, InGraph)
		, CachedPlane(ForceInitToZero)
	{
		TArray<int32> vertexIDs;

		for (const FVector &vertexPos : VertexPositions)
		{
			auto vertex = Graph->FindVertex(vertexPos);
			if (!ensureAlways(vertex != nullptr))
			{
				bValid = false;
				vertexIDs.Reset();
				return;
			}

			vertexIDs.Add(vertex->ID);
		}

		UpdatePlane(VertexIDs);
		UpdateVerticesAndEdges(vertexIDs, false);
	}

	FVector2D FGraph3DFace::ProjectPosition2D(const FVector &Position) const
	{
		if (!ensureAlways(CachedPositions.Num() > 0))
		{
			return FVector2D::ZeroVector;
		}

		return UModumateGeometryStatics::ProjectPoint2D(Position, Cached2DX, Cached2DY, CachedPositions[0]);
	}

	FVector FGraph3DFace::DeprojectPosition(const FVector2D &ProjectedPos) const
	{
		if (!ensureAlways(Cached2DX.IsNormalized() && Cached2DY.IsNormalized() &&
			FVector::Orthogonal(Cached2DX, Cached2DY) && (CachedPositions.Num() > 0)))
		{
			return FVector::ZeroVector;
		}

		return CachedPositions[0] + (ProjectedPos.X * Cached2DX) + (ProjectedPos.Y * Cached2DY);
	}

	bool FGraph3DFace::AssignVertices(const TArray<int32> &InVertexIDs)
	{
		VertexIDs = InVertexIDs;
		int32 numVertices = VertexIDs.Num();
		if (!ensureAlways(numVertices >= 3))
		{
			bValid = false;
			return bValid;
		}

		// Get the positions of all the vertices used for this face's polygon
		CachedPositions.Reset(numVertices);
		for (int32 vertexID : VertexIDs)
		{
			FGraph3DVertex *vertex = Graph->FindVertex(vertexID);
			if (!ensureAlways(vertex != nullptr))
			{
				bValid = false;
				return bValid;
			}

			CachedPositions.Add(vertex->Position);
		}

		return true;
	}

	bool FGraph3DFace::UpdatePlane(const TArray<int32> &InVertexIDs)
	{
		if (!AssignVertices(InVertexIDs))
		{
			return false;
		}

		bool bPlanar = UModumateGeometryStatics::GetPlaneFromPoints(CachedPositions, CachedPlane);

		if (!(bPlanar && CachedPlane.IsNormalized()))
		{
			return false;
		}

		return true;
	}

	bool FGraph3DFace::UpdateVerticesAndEdges(const TArray<int32> &InVertexIDs, bool bAssignVertices)
	{
		if (bAssignVertices)
		{
			if (!AssignVertices(InVertexIDs))
			{
				return false;
			}
		}

		int32 numVertices = VertexIDs.Num();

		if (!UModumateGeometryStatics::AnalyzeCachedPositions(CachedPositions, CachedPlane, Cached2DX, Cached2DY, Cached2DPositions, CachedCenter, false))
		{
			bValid = false;
			return bValid;
		}

		// Find/add the edges that are formed from the pairs of vertices in this face's polygon
		for (auto edgeID : EdgeIDs)
		{
			auto edge = Graph->FindEdge(edgeID);
			if (edge != nullptr)
			{
				edge->RemoveFace(ID, false);
			}
		}

		EdgeIDs.Reset(numVertices);
		CachedEdgeNormals.Reset(numVertices);
		for (int32 idxA = 0; idxA < numVertices; ++idxA)
		{
			int32 idxB = (idxA + 1) % numVertices;
			int32 vertexIDA = VertexIDs[idxA];
			int32 vertexIDB = VertexIDs[idxB];

			bool bEdgeForward = true;
			auto edge = Graph->FindEdgeByVertices(vertexIDA, vertexIDB, bEdgeForward);
			if (edge == nullptr)
			{
				bValid = false;
				return bValid;
			}

			FSignedID edgeID = edge->ID * (bEdgeForward ? 1 : -1);
			EdgeIDs.Add(edgeID);

			FVector edgeDir = edge->CachedDir * (bEdgeForward ? 1.0f : -1.0f);
			FVector edgeNormal = CachedPlane ^ edgeDir;
			CachedEdgeNormals.Add(edgeNormal);

			FSignedID signedFaceID = ID * (bEdgeForward ? 1 : -1);
			edge->AddFace(signedFaceID, edgeNormal);
		}

		bValid = true;
		return bValid;
	}

	bool FGraph3DFace::NormalHitsFace(const FGraph3DFace *OtherFace, float Sign) const
	{
		FVector rayDir(CachedPlane);
		rayDir *= Sign;

		if (FVector::Orthogonal(rayDir, OtherFace->CachedPlane))
		{
			return false;
		}

		FVector intersectionPoint = FMath::RayPlaneIntersection(CachedCenter, rayDir, OtherFace->CachedPlane);
		FVector intersectionDelta = intersectionPoint - CachedCenter;
		float intersectionDist = intersectionDelta | rayDir;
		if (intersectionDist <= 0.0f)
		{
			return false;
		}

		FVector2D intersection2D = OtherFace->ProjectPosition2D(intersectionPoint);
		return UModumateFunctionLibrary::PointInPoly2D(intersection2D, OtherFace->Cached2DPositions);
	}

	bool FGraph3DFace::IntersectsFace(const FGraph3DFace *OtherFace, FVector &IntersectingEdgeOrigin, FVector &IntersectingEdgeDir, TArray<FEdgeIntersection> &SourceIntersections, TArray<FEdgeIntersection> &DestIntersections, TPair<bool, bool> &bOutOnFaceEdge) const
	{
		// Make sure that this faces plane at least intersects with the other face, before finding shared edge segments
		if (!FMath::IntersectPlanes2(IntersectingEdgeOrigin, IntersectingEdgeDir, CachedPlane, OtherFace->CachedPlane))
		{
			return false;
		}

		// Given the line that these two faces' planes share, find the points where the line intersects their edges, if any
		UModumateGeometryStatics::GetEdgeIntersections(CachedPositions, IntersectingEdgeOrigin, IntersectingEdgeDir, SourceIntersections, Graph->Epsilon);
		UModumateGeometryStatics::GetEdgeIntersections(OtherFace->CachedPositions, IntersectingEdgeOrigin, IntersectingEdgeDir, DestIntersections, Graph->Epsilon);

		// TODO: only allows one intersecting segment per face
		if (SourceIntersections.Num() != 2)
		{
			return false;
		}

		TArray<FEdgeIntersection> trimmedDestIntersections;
		for (auto sourceIntersection : SourceIntersections)
		{
			for (auto destIntersection : DestIntersections)
			{
				if (FMath::Abs(sourceIntersection.DistAlongRay - destIntersection.DistAlongRay) < Graph->Epsilon)
				{
					trimmedDestIntersections.Add(destIntersection);
				}
			}
		}

		if (trimmedDestIntersections.Num() != 2)
		{
			return false;
		}

		DestIntersections = trimmedDestIntersections;

		// return false if faces share an edge, so neither face should be split
		bool bFacesShareEdge = false;

		// TODO: change this when more than one intersection segment is allowed per face
		auto vertexA = Graph->FindVertex(SourceIntersections[0].Point);
		auto vertexB = Graph->FindVertex(SourceIntersections[1].Point);

		if (vertexA != nullptr && vertexB != nullptr)
		{
			bool bForward;
			auto edge = Graph->FindEdgeByVertices(vertexA->ID, vertexB->ID, bForward);
			if (edge != nullptr)
			{
				bool bFaceContainsEdge = EdgeIDs.Contains(edge->ID) || EdgeIDs.Contains(edge->ID * -1);
				bool bOtherFaceContainsEdge = OtherFace->EdgeIDs.Contains(edge->ID) || OtherFace->EdgeIDs.Contains(edge->ID * -1);
				bOutOnFaceEdge.Key = bFaceContainsEdge;
				bOutOnFaceEdge.Value = bOtherFaceContainsEdge;
			}
			// if the edge doesn't exist directly, see if other edges sum to the edge between the intersections by 
			// attempting to add an edge there
			else
			{
				FGraph3DDelta testAddEdgeDelta;
				TArray<int32> OutVertexIDs;
				TArray<int32> parentIDs;
				int32 NextID, ExistingID;
				Graph->GetDeltaForMultipleEdgeAdditions(Graph, FVertexPair(vertexA->ID, vertexB->ID), testAddEdgeDelta, NextID, ExistingID, OutVertexIDs, parentIDs);

				if (testAddEdgeDelta.EdgeAdditions.Num() == 0)
				{
					return false;
				}
			}

			// if faces don't contain edge, make sure that the edge is inside the face
			int32 intersectionIdx = 0;
			for (auto intersection : { DestIntersections, SourceIntersections })
			{
				if ((intersectionIdx == 0 && !bOutOnFaceEdge.Value) || (intersectionIdx == 1 && !bOutOnFaceEdge.Key))
				{
					FVector diff = (intersection[1].Point - intersection[0].Point).GetSafeNormal();

					// idx is iterating across both directions of the edge
					for (int32 idx = 0; idx < 2; idx++)
					{
						// TODO: faces and intersections are created so that EdgeIdxA is the necessary value here
						// EdgeIdxB is currently unused and may be unnecessary
						int32 edgeIdx = intersection[idx].EdgeIdxA;
						FVector edgeNormal = intersectionIdx == 0 ? OtherFace->CachedEdgeNormals[edgeIdx] : CachedEdgeNormals[edgeIdx];

						// flip the intersection direction to evaluate the second vertex of the intersection
						if (idx == 1)
						{
							diff *= -1.0f;
						}

						// edge normals are pointing towards the interior of the face, so the intersection is outside of the face
						// if the projection is in the opposite direction of the edge normal
						if ((diff | edgeNormal) < 0)
						{
							return false;
						}
					}
				}
				intersectionIdx++;
			}
		}

		return true;
	}

	bool FGraph3DFace::IntersectsPlane(const FPlane OtherPlane, FVector &IntersectingEdgeOrigin, FVector &IntersectingEdgeDir, TArray<TPair<FVector, FVector>> &IntersectingSegments) const
	{
		if (!FMath::IntersectPlanes2(IntersectingEdgeOrigin, IntersectingEdgeDir, CachedPlane, OtherPlane))
		{
			return false;
		}

		TArray<FEdgeIntersection> sourceEdgeIntersections;
		UModumateGeometryStatics::GetEdgeIntersections(CachedPositions, IntersectingEdgeOrigin, IntersectingEdgeDir, sourceEdgeIntersections, Graph->Epsilon);

		// Because planes have infinite dimensions, the plane must exit the face before it can enter again, and the source list is sorted,
		// the line segments are defined by groups of two intersections.
		for (int segmentIdx = 0; segmentIdx < sourceEdgeIntersections.Num() - 1; segmentIdx += 2)
		{
			IntersectingSegments.Add(TPair<FVector, FVector>(sourceEdgeIntersections[segmentIdx].Point, sourceEdgeIntersections[segmentIdx + 1].Point));
		}

		return true;
	}

	float FGraph3DFace::CalculateArea()
	{
		TArray<FPolyHole2D> holes;
		TArray<FVector2D> vertices, OutPerimeter;
		TArray<int32> triangles, outholeidxs;
		TArray<bool> outholes;
		UModumateGeometryStatics::TriangulateVerticesPoly2Tri(Cached2DPositions, holes, vertices, triangles, OutPerimeter, outholes, outholeidxs);

		float area = 0.0f;
		for (int32 i = 0; i < triangles.Num(); i += 3)
		{
			const auto &p1 = vertices[triangles[i]], &p2 = vertices[triangles[i + 1]], &p3 = vertices[triangles[i + 2]];

			FVector2D triBaseDelta = p2 - p1;
			float triBaseLen = triBaseDelta.Size();
			float triHeight = FMath::PointDistToLine(FVector(p3.X, p3.Y, 0.0f), FVector(triBaseDelta.X, triBaseDelta.Y, 0.0f), FVector(p2.X, p2.Y, 0.0f));
			float triArea = 0.5f * triBaseLen * triHeight;

			area += triArea;
		}

		return area;
	}

	void FGraph3DFace::GetAdjacentFaceIDs(TSet<int32>& OutFaceIDs) const
	{
		for (int32 edgeID : EdgeIDs)
		{
			auto edge = Graph->FindEdge(edgeID);
			if (edge != nullptr)
			{
				for (auto faceConnection : edge->ConnectedFaces)
				{
					if (FMath::Abs(faceConnection.FaceID) != FMath::Abs(ID))
					{
						OutFaceIDs.Add(FMath::Abs(faceConnection.FaceID));
					}
				}
			}
		}
	}

	const bool FGraph3DFace::FindVertex(FVector &Position) const
	{
		for (auto position : CachedPositions)
		{
			if (position.Equals(Position, DEFAULT_GRAPH3D_EPSILON))
			{
				return true;
			}
		}

		return false;
	}

	const int32 FGraph3DFace::FindEdgeIndex(FSignedID edgeID, bool &bOutSameDirection) const
	{
		for (int32 edgeIdx = 0; edgeIdx < EdgeIDs.Num(); edgeIdx++)
		{
			int32 id = EdgeIDs[edgeIdx];
			if (FMath::Abs(id) == FMath::Abs(edgeID))
			{
				bOutSameDirection = (id == edgeID);
				return edgeIdx;
			}
		}
		return INDEX_NONE;
	}

	const bool FGraph3DFace::SplitFace(TArray<int32> EdgeIdxs, TArray<int32> NewVertexIDs, TArray<TArray<int32>>& OutVertexIDs) const
	{
		// malformed inputs
		if (EdgeIdxs.Num() != NewVertexIDs.Num())
		{
			return false;
		}
		if (EdgeIdxs.Num() != 2)
		{
			return false;
		}

		OutVertexIDs.Reset();
		OutVertexIDs.Init(TArray<int32>(), 2);

		int32 startSplitEdgeID = EdgeIdxs[0];
		int32 endSplitEdgeID = EdgeIdxs[1];

		for (int32 vertexIdx = 0; vertexIdx < VertexIDs.Num(); vertexIdx++)
		{
			if (vertexIdx <= startSplitEdgeID)
			{
				OutVertexIDs[0].Add(VertexIDs[vertexIdx]);
			}
			if (vertexIdx == startSplitEdgeID)
			{
				if (VertexIDs[vertexIdx] != NewVertexIDs[0])
				{
					OutVertexIDs[0].Add(NewVertexIDs[0]);
				}
				OutVertexIDs[1].Add(NewVertexIDs[0]);
			}
			if (vertexIdx > startSplitEdgeID && vertexIdx <= endSplitEdgeID)
			{
				OutVertexIDs[1].Add(VertexIDs[vertexIdx]);
			}
			if (vertexIdx == endSplitEdgeID)
			{
				OutVertexIDs[0].Add(NewVertexIDs[1]);
				if (VertexIDs[vertexIdx] != NewVertexIDs[1])
				{
					OutVertexIDs[1].Add(NewVertexIDs[1]);
				}
			}
			if (vertexIdx > endSplitEdgeID)
			{
				OutVertexIDs[0].Add(VertexIDs[vertexIdx]);
			}
		}

		return true;
	}

	void FGraph3DFace::Dirty(bool bConnected)
	{
		bDirty = true;

		if (bConnected)
		{
			bool bContinueConnected = false;

			for (int32 vertexID : VertexIDs)
			{
				auto vertex = Graph->FindVertex(vertexID);
				if (vertex == nullptr)
				{
					continue;
				}

				vertex->Dirty(bContinueConnected);
			}

			for (int32 edgeID : EdgeIDs)
			{
				auto edge = Graph->FindEdge(edgeID);
				if (edge == nullptr)
				{
					continue;
				}

				edge->Dirty(bContinueConnected);
			}
		}
	}

	bool FGraph3DFace::ValidateForTests() const
	{
		if (!bValid)
		{
			return false;
		}

		if (Graph == nullptr)
		{
			return false;
		}

		for (int32 vertexID : VertexIDs)
		{
			auto vertex = Graph->FindVertex(vertexID);
			if (vertex == nullptr)
			{
				return false;
			}
		}

		for (int32 edgeID : EdgeIDs)
		{
			auto edge = Graph->FindEdge(edgeID);
			if (edge == nullptr)
			{
				return false;
			}
		}

		return true;
	}
}
