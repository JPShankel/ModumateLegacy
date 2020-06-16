#include "Graph/Graph3DFace.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Graph/ModumateGraph.h"

namespace Modumate
{
	FGraph3DFace::FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<int32> &InVertexIDs,
		const TSet<int32> &InGroupIDs, int32 InContainingFaceID, const TArray<int32> &InContainedFaceIDs)
		: IGraph3DObject(InID, InGraph, InGroupIDs)
		, ContainingFaceID(InContainingFaceID)
		, ContainedFaceIDs(InContainedFaceIDs)
		, CachedPlane(ForceInitToZero)
	{
		UpdatePlane(InVertexIDs);
		UpdateVerticesAndEdges(InVertexIDs, false);
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

	bool FGraph3DFace::ContainsPosition(const FVector &Position, FVector2D &OutProjectedPos) const
	{
		float planeDist = CachedPlane.PlaneDot(Position);
		if (!FMath::IsNearlyZero(planeDist, Graph->Epsilon))
		{
			return false;
		}

		OutProjectedPos = ProjectPosition2D(Position);
		return UModumateGeometryStatics::IsPointInPolygon(OutProjectedPos, Cached2DPositions, Graph->Epsilon, false);
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

	bool FGraph3DFace::ShouldUpdatePlane() const
	{
		for (int32 vertexID : VertexIDs)
		{
			auto vertex = Graph->FindVertex(vertexID);
			if (!ensureAlways(vertex != nullptr))
			{
				continue;
			}

			if (FMath::Abs(CachedPlane.PlaneDot(vertex->Position)) > KINDA_SMALL_NUMBER)
			{
				return true;
			}
		}
		return false;
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

		int32 numVertices = VertexIDs.Num();
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

		CalculateArea();

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

	bool FGraph3DFace::IntersectsFace(const FGraph3DFace *OtherFace, TArray<TPair<FVector, FVector>> &OutEdges) const
	{
		FVector IntersectingEdgeOrigin, IntersectingEdgeDir;
		TArray<FEdgeIntersection> SourceIntersections, DestIntersections;

		// Make sure that this faces plane at least intersects with the other face, before finding shared edge segments
		if (!FMath::IntersectPlanes2(IntersectingEdgeOrigin, IntersectingEdgeDir, CachedPlane, OtherFace->CachedPlane))
		{
			return false;
		}

		// Given the line that these two faces' planes share, find the points where the line intersects their edges, if any
		UModumateGeometryStatics::GetEdgeIntersections(CachedPositions, IntersectingEdgeOrigin, IntersectingEdgeDir, SourceIntersections, Graph->Epsilon);
		UModumateGeometryStatics::GetEdgeIntersections(OtherFace->CachedPositions, IntersectingEdgeOrigin, IntersectingEdgeDir, DestIntersections, Graph->Epsilon);

		// TODO: ensure that GetEdgeIntersections can only return an even amount of intersections through a unit test
		if (SourceIntersections.Num() % 2 != 0 || DestIntersections.Num() % 2 != 0)
		{
			return false;
		}

		// the faces intersect, and an edge needs to be drawn, across the ranges that are in both the source and dest intersections
		// a range is the area along the ray that intersects with the face
		TArray<TPair<float, float>> sourceRanges;
		for (int32 sourceIdx = 0; sourceIdx < SourceIntersections.Num(); sourceIdx+=2)
		{
			sourceRanges.Add(TPair<float, float>(SourceIntersections[sourceIdx].DistAlongRay, SourceIntersections[sourceIdx + 1].DistAlongRay));
		}

		TArray<TPair<float, float>> destRanges;
		for (int32 destIdx = 0; destIdx < DestIntersections.Num(); destIdx+=2)
		{
			destRanges.Add(TPair<float, float>(DestIntersections[destIdx].DistAlongRay, DestIntersections[destIdx + 1].DistAlongRay));
		}

		// Takes the source ranges and dest ranges and merges them into a list of ranges that are in both
		int32 sourceIdx = 0;
		int32 destIdx = 0;
		TArray<TPair<float, float>> outRanges;

		while (sourceIdx < sourceRanges.Num() && destIdx < destRanges.Num())
		{
			float sourceMin = sourceRanges[sourceIdx].Key;
			float sourceMax = sourceRanges[sourceIdx].Value;

			float destMin = destRanges[destIdx].Key;
			float destMax = destRanges[destIdx].Value;

			if (sourceMax >= destMin && destMax >= sourceMin)
			{
				TArray<float> vals = { sourceMin, sourceMax, destMin, destMax };
				vals.Sort();
	
				// (vals[0], vals[1]) and (vals[2], vals[3]) could potentially be added as well to create edges that 
				// go all the way across the faces.
				// TODO: using these values would require some input validation that is missing
				outRanges.Add(TPair<float, float>(vals[1], vals[2]));
			}

			if (sourceMax < destMax)
			{
				sourceIdx++;
			}
			else
			{
				destIdx++;
			}
		}

		// convert from ranges to edges
		for (auto& range : outRanges)
		{
			FVector edgeStart = IntersectingEdgeOrigin + IntersectingEdgeDir * range.Key;
			FVector edgeEnd = IntersectingEdgeOrigin + IntersectingEdgeDir * range.Value;

			OutEdges.Add(TPair<FVector, FVector>(edgeStart, edgeEnd));
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

	void FGraph3DFace::CalculateArea()
	{
		// create 2D graph
		FGraph graph2D;
		for (int32 vertexIdx = 0; vertexIdx < VertexIDs.Num(); vertexIdx++)
		{
			graph2D.AddVertex(Cached2DPositions[vertexIdx], VertexIDs[vertexIdx]);
		}
		for (int32 edgeID : EdgeIDs)
		{
			auto edge = Graph->FindEdge(edgeID);
			if (!ensureAlways(edge != nullptr)) return;
			graph2D.AddEdge(edge->StartVertexID, edge->EndVertexID, edge->ID);
		}

		graph2D.CalculatePolygons();
		CachedPerimeter.Reset();
		CachedHoles.Reset();

		for (auto& kvp : graph2D.GetPolygons())
		{
			auto& poly = kvp.Value;
			if (!poly.bInterior)
			{
				CachedPerimeter = poly.Points;
			}
			else
			{
				bool bEnclosedPoly = true;
				for (auto& edgeID : poly.Edges)
				{
					auto edge = graph2D.FindEdge(edgeID);
					auto leftPoly = graph2D.FindPolygon(edge->LeftPolyID);
					auto rightPoly = graph2D.FindPolygon(edge->RightPolyID);
					if (!ensure(leftPoly != nullptr && rightPoly != nullptr))
					{
						continue;
					}
					if (!leftPoly->bInterior || !rightPoly->bInterior)
					{
						bEnclosedPoly = false;
						break;
					}
				}
				if (bEnclosedPoly)
				{
					CachedHoles.Add(FPolyHole2D(poly.Points));
				}
			}
		}

		TArray<FVector2D> OutVertices, OutPerimeter;
		TArray<int32> OutTriangles, outholeidxs;
		TArray<bool> outholes;
		UModumateGeometryStatics::TriangulateVerticesPoly2Tri(CachedPerimeter, CachedHoles, OutVertices, OutTriangles, OutPerimeter, outholes, outholeidxs);

		CachedArea = 0.0f;
		for (int32 i = 0; i < OutTriangles.Num(); i += 3)
		{
			const auto &p1 = OutVertices[OutTriangles[i]], &p2 = OutVertices[OutTriangles[i + 1]], &p3 = OutVertices[OutTriangles[i + 2]];

			FVector2D triBaseDelta = p2 - p1;
			float triBaseLen = triBaseDelta.Size();
			float triHeight = FMath::PointDistToLine(FVector(p3.X, p3.Y, 0.0f), FVector(triBaseDelta.X, triBaseDelta.Y, 0.0f), FVector(p2.X, p2.Y, 0.0f));
			float triArea = 0.5f * triBaseLen * triHeight;

			CachedArea += triArea;
		}
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
