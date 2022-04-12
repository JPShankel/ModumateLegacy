#include "Graph/Graph3DFace.h"
#include "Graph/Graph3D.h"
#include "Graph/Graph3DDelta.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Graph/Graph2D.h"
#include "Polygon2.h"
#include "Algo/Accumulate.h"

FGraph3DFace::FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<int32> &InVertexIDs, int32 InContainingFaceID, const TSet<int32> &InContainedFaceIDs) : IGraph3DObject(InID, InGraph)
	, ContainingFaceID(InContainingFaceID)
	, ContainedFaceIDs(InContainedFaceIDs)
{
	if (!UpdatePlane(InVertexIDs))
	{
		bValid = false;
		return;
	}

	UpdateVerticesAndEdges(InVertexIDs, false);
}

FGraph3DFace::FGraph3DFace() : IGraph3DObject(0,nullptr)
{
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

bool FGraph3DFace::ContainsPosition(const FVector &Position, bool& bOutOverlaps) const
{
	bOutOverlaps = false;
	float planeDist = CachedPlane.PlaneDot(Position);
	if (!FMath::IsNearlyZero(planeDist, Graph->Epsilon))
	{
		return false;
	}

	FPointInPolyResult pointInPolyResult;
	if (!ensure(UModumateGeometryStatics::TestPointInPolygon(ProjectPosition2D(Position), Cached2DPositions, pointInPolyResult, Graph->Epsilon)))
	{
		return false;
	}

	bOutOverlaps = pointInPolyResult.bOverlaps;
	return pointInPolyResult.bInside;
}

bool FGraph3DFace::UpdatePlane(const TArray<int32> &InVertexIDs)
{
	if (!AssignVertices(InVertexIDs))
	{
		return false;
	}

	bool bPlanar = UModumateGeometryStatics::GetPlaneFromPoints(CachedPositions, CachedPlane, Graph->Epsilon * 2.0f);

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
			bValid = false;
			return bValid;
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
			edge->RemoveFace(ID, false, false);
		}
	}

	int32 numVertices = VertexIDs.Num();
	EdgeIDs.Reset(numVertices);
	CachedEdgeNormals.Reset(numVertices);
	Cached2DEdgeNormals.Reset(numVertices);
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

		FGraphSignedID edgeID = edge->ID * (bEdgeForward ? 1 : -1);
		EdgeIDs.Add(edgeID);

		// TODO: it is possible that the edge hasn't been cleaned by this point, so the CachedDir is wrong
		edge->SetVertices(edge->StartVertexID, edge->EndVertexID);

		FVector edgeDir = edge->CachedDir * (bEdgeForward ? 1.0f : -1.0f);
		FVector edgeNormal = CachedPlane ^ edgeDir;
		if (!ensure(!edgeNormal.IsNearlyZero()))
		{
			bValid = false;
			return bValid;
		}

		edgeNormal.Normalize();
		CachedEdgeNormals.Add(edgeNormal);

		// Also cache the 2D edge normal here, which should already be safe to project into the plane and stay normalized.
		FVector2D edgeNormal2D = UModumateGeometryStatics::ProjectVector2D(edgeNormal, Cached2DX, Cached2DY);
		bool edgeNormal2DNormalized = (FMath::Abs(1.f - edgeNormal2D.SizeSquared()) < THRESH_VECTOR_NORMALIZED);
		if (!ensure(edgeNormal2DNormalized))
		{
			bValid = false;
			return bValid;
		}
		Cached2DEdgeNormals.Add(edgeNormal2D);

		FGraphSignedID signedFaceID = ID * (bEdgeForward ? 1 : -1);
		edge->AddFace(signedFaceID, edgeNormal, false);
	}
	UpdateContainingFace(ContainingFaceID);

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
	FPointInPolyResult pointInPolyResult;
	bool bTestSuccess = UModumateGeometryStatics::TestPointInPolygon(intersection2D, OtherFace->Cached2DPositions, pointInPolyResult);
	return bTestSuccess && (pointInPolyResult.bInside || pointInPolyResult.bOverlaps);
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

		if (FMath::IsNearlyEqual(range.Key, range.Value, Graph->Epsilon))
		{
			return false;
		}

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

bool FGraph3DFace::UpdateHoles()
{
	CachedHoles.Reset();
	Cached2DHoles.Reset();

	bool bSuccess = true;
	TArray<FVector> holePoints;
	TArray<FVector2D> holePoints2D;
	for (int32 containedFaceID : ContainedFaceIDs)
	{
		// The contained face may not be in the graph yet if we are mid-delta
		// TODO: only and always call this function when necessary, and when all referenced faces are in the graph
		auto containedFace = Graph->FindFace(containedFaceID);
		if ((containedFace == nullptr) || !ensure(containedFace->CachedPositions.Num() >= 3))
		{
			bSuccess = false;
			continue;
		}

		FPolyHole3D& containedFaceHole = CachedHoles.AddDefaulted_GetRef();
		FPolyHole2D& containedFace2DHole = Cached2DHoles.AddDefaulted_GetRef();

		for (FVector& position : containedFace->CachedPositions)
		{
			containedFaceHole.Points.Add(position);
			containedFace2DHole.Points.Add(ProjectPosition2D(position));
		}
	}

	return bSuccess;
}

void FGraph3DFace::UpdateContainingFace(int32 NewContainingFaceID)
{
	for (int32 edgeID : EdgeIDs)
	{
		auto edge = Graph->FindEdge(edgeID);
		if (edge == nullptr)
		{
			continue;
		}

		FVector edgeNormal;
		int32 sign = 1;
		for (auto& connection : edge->ConnectedFaces)
		{
			if (FMath::Abs(connection.FaceID) == ID)
			{
				sign = connection.FaceID == ID ? 1 : -1;
				edgeNormal = connection.EdgeFaceDir * -1.0f;
			}
		}

		if (!edgeNormal.IsNearlyZero())
		{
			edge->RemoveParallelContainingFace(edgeNormal);
		}

		// don't add a face connection when the new face doesn't exist
		if (NewContainingFaceID == MOD_ID_NONE)
		{
			continue;
		}

		edge->AddFace(NewContainingFaceID * sign, edgeNormal, true);
	}

	ContainingFaceID = NewContainingFaceID;
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

int32 FGraph3DFace::FindVertexID(const FVector &Position) const
{
	int32 numVertices = VertexIDs.Num();
	if (numVertices != CachedPositions.Num())
	{
		return MOD_ID_NONE;
	}

	for (int32 i = 0; i < numVertices; ++i)
	{
		if (CachedPositions[i].Equals(Position, DEFAULT_GRAPH3D_EPSILON))
		{
			return VertexIDs[i];
		}
	}

	return MOD_ID_NONE;
}

int32 FGraph3DFace::FindEdgeIndex(FGraphSignedID edgeID, bool &bOutSameDirection) const
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

void FGraph3DFace::GetVertexIDs(TArray<int32>& OutVertexIDs) const
{
	OutVertexIDs = VertexIDs;
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

double FGraph3DFace::CalculateArea() const
{
	auto verticesToTPoly = [](const TArray<FVector2D>& Vertices)
	{
		TArray<FVec2d> convertedVertices;
		Algo::Transform(Vertices, convertedVertices, [](const FVector2D& Vector) {return Vector; });
		return FPolygon2d(convertedVertices).Area();
	};

	return verticesToTPoly(Cached2DPositions) - Algo::TransformAccumulate(Cached2DHoles, [verticesToTPoly](const FPolyHole2D& hole) { return verticesToTPoly(hole.Points); }, 0.0);
}

