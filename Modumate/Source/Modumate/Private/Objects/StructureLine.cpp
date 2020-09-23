// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/StructureLine.h"

#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Algo/ForEach.h"

FMOIStructureLine::FMOIStructureLine(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, LineStartPos(ForceInitToZero)
	, LineEndPos(ForceInitToZero)
	, LineDir(ForceInitToZero)
	, LineNormal(ForceInitToZero)
	, LineUp(ForceInitToZero)
	, UpperExtensions(ForceInitToZero)
	, OuterExtensions(ForceInitToZero)
{
}

FMOIStructureLine::~FMOIStructureLine()
{
}

FQuat FMOIStructureLine::GetRotation() const
{
	FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObj)
	{
		return parentObj->GetObjectRotation();
	}

	return FQuat::Identity;
}

FVector FMOIStructureLine::GetLocation() const
{
	FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObj)
	{
		return parentObj->GetObjectLocation();
	}

	return FVector::ZeroVector;
}

void FMOIStructureLine::SetupDynamicGeometry()
{
	InternalUpdateGeometry(true, true);
}

void FMOIStructureLine::UpdateDynamicGeometry()
{
	InternalUpdateGeometry(false, true);
}

void FMOIStructureLine::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const FSimplePolygon* profile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(&MOI->GetAssembly(), profile))
	{
		return;
	}

	// For snapping, we want to attach to the underlying line itself
	if (bForSnapping)
	{
		outPoints.Add(FStructurePoint(LineStartPos, FVector::ZeroVector, 0));
		outPoints.Add(FStructurePoint(LineEndPos, FVector::ZeroVector, 1));

		outLines.Add(FStructureLine(LineStartPos, LineEndPos, 0, 1));
	}
	// Otherwise, we want the extents of the mesh
	else
	{
		FVector2D profileSize = profile->Extents.GetSize();

		FVector centroid = 0.5f * (LineStartPos + LineEndPos);
		FVector boxExtents(FVector::Dist(LineStartPos, LineEndPos), profileSize.X, profileSize.Y);
		FQuat boxRot = FRotationMatrix::MakeFromYZ(LineUp, LineNormal).ToQuat();

		FModumateSnappingView::GetBoundingBoxPointsAndLines(centroid, boxRot, 0.5f * boxExtents, outPoints, outLines);
	}
}

void FMOIStructureLine::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
{
	if (!ensure(DynamicMeshActor.IsValid() && MOI && (MOI->GetAssembly().Extrusions.Num() == 1)))
	{
		return;
	}

	// This can be an expected error, if the object is still getting set up before it has a parent assigned.
	const FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObj == nullptr)
	{
		return;
	}

	LineStartPos = parentObj->GetCorner(0);
	LineEndPos = parentObj->GetCorner(1);
	LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
	UModumateGeometryStatics::FindBasisVectors(LineNormal, LineUp, LineDir);

	DynamicMeshActor->SetupExtrudedPolyGeometry(MOI->GetAssembly(), LineStartPos, LineEndPos,
		LineNormal, LineUp, UpperExtensions, OuterExtensions, FVector::OneVector, bRecreate, bCreateCollision);
}

void FMOIStructureLine::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	static const Modumate::Units::FThickness lineThickness = Modumate::Units::FThickness::Points(0.25f);
	static const Modumate::FMColor lineColor = Modumate::FMColor::Black;
	static Modumate::FModumateLayerType dwgLayerType = Modumate::FModumateLayerType::kCabinetCutCarcass;
	OutPerimeters.Reset();

	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{   // In cut-plane lines.
		TArray<FVector> perimeter;
		if (!GetPerimeterPoints(perimeter))
		{
			return;
		}

		const FVector position = MOI->GetObjectLocation();
		const FVector startOffset = LineStartPos - position;
		const FVector endOffset = LineEndPos - position;
		FVector startCap(perimeter.Last(0) + startOffset);
		FVector endCap(perimeter.Last(0) + endOffset);

		TArray<FVector2D> points;
		TArray<FVector2D> startCapPoints;
		TArray<FVector2D> endCapPoints;
		for (const auto& edgePoint: perimeter)
		{
			FVector p1(edgePoint + startOffset);
			FVector p2(edgePoint + endOffset);
			FVector intersect;
			if (FMath::SegmentPlaneIntersection(p1, p2, Plane, intersect))
			{
				points.Emplace(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersect));
			}
			if (FMath::SegmentPlaneIntersection(startCap, p1, Plane, intersect))
			{
				startCapPoints.Emplace(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersect));
			}
			if (FMath::SegmentPlaneIntersection(endCap, p2, Plane, intersect))
			{
				endCapPoints.Emplace(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersect));
			}
			startCap = p1;
			endCap = p2;
		}

		if (points.Num() + startCapPoints.Num() + endCapPoints.Num() >= 2)
		{
			TArray<FEdge> draftEdges;
			int32 numPoints = points.Num();
			if (numPoints >= 2)
			{
				for (int32 p = 0; p < numPoints; ++p)
				{
					draftEdges.Emplace(FVector(points[p], 0), FVector(points[(p + 1) % numPoints], 0));
				}
			}

			if (startCapPoints.Num() == endCapPoints.Num() && startCapPoints.Num() >= 2)
			{
				numPoints = startCapPoints.Num();
				for (int32 p = 0; p < numPoints; ++p)
				{
					draftEdges.Emplace(FVector(startCapPoints[p], 0), FVector(endCapPoints[p], 0));
					draftEdges.Emplace(FVector(startCapPoints[p], 0), FVector(startCapPoints[(p + 1) % numPoints], 0));
					draftEdges.Emplace(FVector(endCapPoints[p], 0), FVector(endCapPoints[(p + 1) % numPoints], 0));
				}
			}

			for (const auto& edge: draftEdges)
			{
				FVector2D vert0(edge.Vertex[0]);
				FVector2D vert1(edge.Vertex[1]);

				FVector2D boxClipped0;
				FVector2D boxClipped1;

				if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
				{
					TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
						Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped0),
						Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped1),
						Modumate::Units::FThickness::Points(0.25f), Modumate::FMColor::Black);
					ParentPage->Children.Add(line);
					line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kBeamColumnCut);
				}

			}
		}
	}
	else
	{   // Beyond cut-plane lines.
		TArray<FEdge> beyondLines = GetBeyondLinesFromMesh(ParentPage, Plane, AxisX, AxisY, Origin);

		TArray<FEdge> clippedLines;
		for (const auto& edge: beyondLines)
		{
			int32 savedCount = edge.Count;
			auto lines = ParentPage->lineClipping->ClipWorldLineToView(edge);
			// Restore count (silhouette flag).
			Algo::ForEach(lines, [savedCount](FEdge& e) {e.Count = savedCount; });
			clippedLines.Append(MoveTemp(lines));
		}

		FVector2D boxClipped0;
		FVector2D boxClipped1;
		for (const auto& clippedLine : clippedLines)
		{
			FVector2D vert0(clippedLine.Vertex[0]);
			FVector2D vert1(clippedLine.Vertex[1]);

			if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
			{
				TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
					Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped0),
					Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped1),
					Modumate::Units::FThickness::Points(bool(clippedLine.Count) ? 0.15f : 0.05f),
					Modumate::FMColor::Gray128);
				ParentPage->Children.Add(line);
				line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kBeamColumnBeyond);
			}
		}

	}
}

bool FMOIStructureLine::GetPerimeterPoints(TArray<FVector>& outPerimeterPoints) const
{
	const FSimplePolygon *polyProfile = nullptr;
	FVector scaleVector;
	FTransform localToWorld = MOI->GetWorldTransform();

	if (!UModumateObjectStatics::GetPolygonProfile(&MOI->GetAssembly(), polyProfile)
		|| polyProfile->Points.Num() == 0)
	{
		return false;
	}
		
	for (const auto& point : polyProfile->Points)
	{
		outPerimeterPoints.Add(localToWorld.TransformPosition(point.X * LineUp + point.Y * LineNormal));
	}

	return true;
}

TArray<FEdge> FMOIStructureLine::GetBeyondLinesFromMesh(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin) const
{
	const FVector viewNormal = Plane;

	TArray<FEdge> beamEdges;

	TArray<FVector> perimeter;
	if (!GetPerimeterPoints(perimeter))
	{
		return beamEdges;
	}

	int32 numEdges = perimeter.Num();
	int32 closestPoint = 0;
	float closestDist = INFINITY;
	for (int32 i = 0; i < numEdges; ++i)
	{
		float dist = Plane.PlaneDot(perimeter[i]);
		if (dist < closestDist)
		{
			closestPoint = i;
			closestDist = dist;
		}
	}

	if (closestDist < 0.0f)
	{
		return beamEdges;
	}
	FVector startPoint(perimeter[closestPoint]);

	const FVector position = MOI->GetObjectLocation();
	const FVector startOffset = LineStartPos - position;
	const FVector endOffset = LineEndPos - position;
	FVector previousNormal(0.0f);
	bool bPreviousFacing = false;

	static constexpr float facetThreshold = 0.985;  // 10 degrees
		
	FVector bottomCap(perimeter[closestPoint] + startOffset);
	FVector topCap(perimeter[closestPoint] + endOffset);

	// Test closest facet for winding direction:
	bool bPerimeterFacingDir;
	FVector tp0 = perimeter[(closestPoint + numEdges - 1) % numEdges];
	FVector tp1 = perimeter[closestPoint];
	FVector tp2 = perimeter[(closestPoint + 1) % numEdges];
	if ( ((tp2 - tp1).GetSafeNormal() | viewNormal) < ((tp0 - tp1).GetSafeNormal() | viewNormal) )
	{
		FVector normal( ((tp2 - tp1) ^ LineDir).GetSafeNormal());
		bPerimeterFacingDir = (normal | viewNormal) > 0;
	}
	else
	{
		FVector normal(((tp1 - tp0) ^ LineDir).GetSafeNormal());
		bPerimeterFacingDir = (normal | viewNormal) > 0;
	}

	for (int32 edge = 0; edge <= numEdges; ++edge)
	{
		FVector p1 = perimeter[(closestPoint + edge) % numEdges];
		FVector p2 = perimeter[(closestPoint + edge + 1) % numEdges];
		FVector normal((p2 - p1) ^ LineDir);
		normal.Normalize();
		bool bFacing = ((normal | viewNormal) < 0) ^ bPerimeterFacingDir;
		if (!previousNormal.IsZero())
		{
			bool bSilhouette = bFacing ^ bPreviousFacing;
			if (bSilhouette || bFacing && (normal | previousNormal) <= facetThreshold)
			{
				FVector draftLineStart(p1 + startOffset);
				FVector draftLineEnd(p1 + endOffset);
				FEdge edgeAlongBeam(draftLineStart, draftLineEnd);
				edgeAlongBeam.Count = int(bSilhouette);  // Use count field to encode internal/silhouette.
				beamEdges.Add(edgeAlongBeam);
				beamEdges.Emplace(bottomCap, draftLineStart);
				beamEdges.Emplace(topCap, draftLineEnd);
				bottomCap = draftLineStart;
				topCap = draftLineEnd;
			}
		}

		previousNormal = normal;
		bPreviousFacing = bFacing;
	}

	return beamEdges;
}
