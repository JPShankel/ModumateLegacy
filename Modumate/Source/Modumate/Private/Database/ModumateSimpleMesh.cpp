// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Database/ModumateSimpleMesh.h"

#include "Graph/Graph2D.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "ModumateCore/ModumateGeometryStatics.h"


void FSimplePolygon::Reset()
{
	Points.Reset();
	Triangles.Reset();
	Extents.Init();
}

void FSimplePolygon::UpdateExtents()
{
	Extents = FBox2D(Points);
}

bool FSimplePolygon::FixUpPerimeter(float Epsilon, FFeedbackContext* InWarn)
{
	// If the polygon is already valid, then there's nothing else to do!
	if (ValidateSimple(nullptr))
	{
		return true;
	}

	// We need at least a triangle to have a valid perimeter for a simple polygon
	int32 numPoints = Points.Num();
	if (numPoints < 3)
	{
		return false;
	}

	// Otherwise, we'll need to use Graph2D analysis to find a perimeter polygon
	auto graph = MakeShared<FGraph2D>(MOD_ID_NONE, Epsilon);
	TArray<int32> vertexIDs;
	TMap<int32, int32> pointIndicesByVertexID;
	for (int32 pointIdx = 0; pointIdx < numPoints; ++pointIdx)
	{
		auto& point = Points[pointIdx];
		auto* vertex = graph->AddVertex(point);
		if (!ensure(vertex && !vertexIDs.Contains(vertex->ID)))
		{
			if (InWarn)
			{
				InWarn->Logf(ELogVerbosity::Error, TEXT("Failed to add new vertex at %s from point #%d"), *point.ToString(), pointIdx);
			}

			return false;
		}

		vertexIDs.Add(vertex->ID);
		pointIndicesByVertexID.Add(vertex->ID, pointIdx);
	}

	int32 numTriIndices = Triangles.Num();
	if (numTriIndices % 3 != 0)
	{
		if (InWarn)
		{
			InWarn->Logf(ELogVerbosity::Error, TEXT("The number of triangle indices (%d) must be a multiple of 3!"), numTriIndices);
		}

		return false;
	}

	int32 numTriangles = numTriIndices / 3;
	for (int32 triIdx = 0; triIdx < numTriangles; ++triIdx)
	{
		int32 triPointIdxA = Triangles[3 * triIdx + 0];
		int32 triPointIdxB = Triangles[3 * triIdx + 1];
		int32 triPointIdxC = Triangles[3 * triIdx + 2];

		if (!ensure(vertexIDs.IsValidIndex(triPointIdxA) && vertexIDs.IsValidIndex(triPointIdxB) && vertexIDs.IsValidIndex(triPointIdxC)))
		{
			if (InWarn)
			{
				InWarn->Logf(ELogVerbosity::Error, TEXT("Triangle #%d referred to points that were not valid vertices!"), triIdx);
			}

			return false;
		}

		int32 vertIdxA = vertexIDs[triPointIdxA];
		int32 vertIdxB = vertexIDs[triPointIdxB];
		int32 vertIdxC = vertexIDs[triPointIdxC];

		bool bEdgeForward;
		if (graph->FindEdgeByVertices(vertIdxA, vertIdxB, bEdgeForward) == nullptr)
		{
			graph->AddEdge(vertIdxA, vertIdxB);
		}
		if (graph->FindEdgeByVertices(vertIdxB, vertIdxC, bEdgeForward) == nullptr)
		{
			graph->AddEdge(vertIdxB, vertIdxC);
		}
		if (graph->FindEdgeByVertices(vertIdxC, vertIdxA, bEdgeForward) == nullptr)
		{
			graph->AddEdge(vertIdxC, vertIdxA);
		}
	}

	graph->CalculatePolygons();

	int32 perimeterPolyID = graph->GetRootExteriorPolyID();
	auto* perimeterPoly = graph->FindPolygon(perimeterPolyID);
	if (!ensure(perimeterPoly && !perimeterPoly->bHasDuplicateVertex && (perimeterPoly->VertexIDs.Num() == numPoints)))
	{
		if (InWarn)
		{
			InWarn->Logf(ELogVerbosity::Error, TEXT("Polygon had no single, comprehensive perimeter traversal with unique vertices!"));
		}

		return false;
	}

	// We've found a unique perimeter polygon; now we can reorder the vertices and triangles so that we only need to store one list of vertices
	TArray<FVector2D> originalPoints = Points;
	TArray<int32> originalTris = Triangles;

	Points.Reset();
	Triangles.Reset();

	TMap<int32, int32> newPointIndicesByOldPointIdx;
	for (int32 vertexIdx = 0; vertexIdx < numPoints; ++vertexIdx)
	{
		int32 vertexID = perimeterPoly->VertexIDs[vertexIdx];
		int32 originalPointIdx = pointIndicesByVertexID[vertexID];
		Points.Add(originalPoints[originalPointIdx]);
		newPointIndicesByOldPointIdx.Add(originalPointIdx, vertexIdx);
	}

	for (int32 originalPointIdx : originalTris)
	{
		int32 newPointIdx = newPointIndicesByOldPointIdx[originalPointIdx];
		Triangles.Add(newPointIdx);
	}

	return ValidateSimple(InWarn);
}

bool FSimplePolygon::ValidateSimple(FFeedbackContext* InWarn) const
{
	// Use a smaller dot epsilon for these polygons, since they're being validated directly from parsed data rather than from geometry calculations.
	static constexpr float polyDotEpsilon = 0.01f;
	return UModumateGeometryStatics::IsPolygon2DValid(Points, InWarn, KINDA_SMALL_NUMBER, polyDotEpsilon);
}

bool USimpleMeshData::SetSourcePath(const FString &FullFilePath)
{
	FString convertedRelPath = FullFilePath;

	SourceRelPath.Empty();
	SourceAbsPath.Empty();

	if (FPaths::MakePathRelativeTo(convertedRelPath, *FPaths::ProjectContentDir()))
	{
		SourceRelPath = convertedRelPath;
		return true;
	}
	else if (!FPaths::IsRelative(FullFilePath) && FPaths::FileExists(FullFilePath))
	{
		SourceAbsPath = FullFilePath;
		return true;
	}

	return false;
}

FString USimpleMeshData::GetSourceFullPath() const
{
	if (!SourceRelPath.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir(), SourceRelPath);
	}
	else if (!SourceAbsPath.IsEmpty())
	{
		return SourceAbsPath;
	}
	else
	{
		return FString();
	}
}
