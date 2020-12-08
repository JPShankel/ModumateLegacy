// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/StructureLine.h"

#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

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
		return parentObj->GetRotation();
	}

	return FQuat::Identity;
}

FVector FMOIStructureLine::GetLocation() const
{
	FModumateObjectInstance *parentObj = MOI ? MOI->GetParentObject() : nullptr;
	if (parentObj)
	{
		return parentObj->GetLocation();
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

void FMOIStructureLine::SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller)
{
	FModumateObjectInstance* parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaEdge)))
	{
		return;
	}

	// edges have two vertices and want an adjustment handle for each
	for (int32 i = 0; i < 2; i++)
	{
		auto cornerHandle = MOI->MakeHandle<AAdjustPolyEdgeHandle>();
		cornerHandle->SetTargetIndex(i);
		cornerHandle->SetTargetMOI(parent);
	}
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
	OutPerimeters.Reset();

	TArray<FVector> perimeter;
	UModumateObjectStatics::GetExtrusionPerimeterPoints(MOI, LineUp, LineNormal, perimeter);

	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{   // In cut-plane lines.
		UModumateObjectStatics::GetExtrusionCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox,
			perimeter, LineStartPos, LineEndPos, Modumate::FModumateLayerType::kBeamColumnCut);
	}
	else
	{   // Beyond cut-plane lines.
		TArray<FEdge> beyondLines = UModumateObjectStatics::GetExtrusionBeyondLinesFromMesh(Plane, perimeter, LineStartPos, LineEndPos);

		TArray<FEdge> clippedLines;
		for (const auto& edge : beyondLines)
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
