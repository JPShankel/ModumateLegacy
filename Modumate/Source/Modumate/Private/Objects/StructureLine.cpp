// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/StructureLine.h"

#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

AMOIStructureLine::AMOIStructureLine()
	: AModumateObjectInstance()
	, LineStartPos(ForceInitToZero)
	, LineEndPos(ForceInitToZero)
	, LineDir(ForceInitToZero)
	, LineNormal(ForceInitToZero)
	, LineUp(ForceInitToZero)
	, UpperExtensions(ForceInitToZero)
	, OuterExtensions(ForceInitToZero)
	, CachedProfileExtents(ForceInitToZero)
{
}

FQuat AMOIStructureLine::GetRotation() const
{
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (parentObj)
	{
		return parentObj->GetRotation();
	}

	return FQuat::Identity;
}

FVector AMOIStructureLine::GetLocation() const
{
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (parentObj)
	{
		return parentObj->GetLocation();
	}

	return FVector::ZeroVector;
}

void AMOIStructureLine::SetupDynamicGeometry()
{
	UpdateCachedGeometry(true, true);
}

void AMOIStructureLine::UpdateDynamicGeometry()
{
	UpdateCachedGeometry(false, true);
}

bool AMOIStructureLine::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIStructureLineData modifiedInstanceData = InstanceData;

	float curFlipSign = modifiedInstanceData.FlipSigns.GetComponentForAxis(FlipAxis);
	modifiedInstanceData.FlipSigns.SetComponentForAxis(FlipAxis, -curFlipSign);

	// If we're not flipping on the Y axis, we also need to flip justification so that flipping across the parent edge can be 1 action/delta.
	if (FlipAxis != EAxis::Y)
	{
		int32 justificationIdx = (FlipAxis == EAxis::Z) ? 0 : 1;
		float& justificationValue = modifiedInstanceData.Justification[justificationIdx];
		justificationValue = (1.0f - justificationValue);
	}

	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}

bool AMOIStructureLine::GetJustifiedState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	FVector2D projectedAdjustments(
		AdjustmentDirection | LineUp,
		AdjustmentDirection | LineNormal
		);
	int32 justificationIdx = (FMath::Abs(projectedAdjustments.X) > FMath::Abs(projectedAdjustments.Y)) ? 0 : 1;
	float projectedAdjustment = projectedAdjustments[justificationIdx];

	OutState = GetStateData();
	FMOIStructureLineData modifiedInstanceData = InstanceData;
	float& justificationValue = modifiedInstanceData.Justification[justificationIdx];

	if (FMath::IsNearlyZero(projectedAdjustment, THRESH_NORMALS_ARE_ORTHOGONAL))
	{
		projectedAdjustment = 0.0f;
	}

	float projectedAdjustmentSign = FMath::Sign(projectedAdjustment);
	float justificationDelta = projectedAdjustmentSign * 0.5f;
	justificationValue = FMath::Clamp(justificationValue + justificationDelta, 0.0f, 1.0f);

	return OutState.CustomData.SaveStructData(modifiedInstanceData);
}

void AMOIStructureLine::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
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
		FVector2D profileSize = CachedProfileExtents.GetSize();
		FVector2D profileCenter = CachedProfileExtents.GetCenter();
		FVector lineCenter = 0.5f * (LineStartPos + LineEndPos) + (profileCenter.X * LineUp) + (profileCenter.Y * LineNormal);
		FVector boxExtents(profileSize.Y, FVector::Dist(LineStartPos, LineEndPos), profileSize.X);
		FQuat boxRot = FRotationMatrix::MakeFromXZ(LineNormal, LineUp).ToQuat();

		FModumateSnappingView::GetBoundingBoxPointsAndLines(lineCenter, boxRot, 0.5f * boxExtents, outPoints, outLines);
	}
}

bool AMOIStructureLine::UpdateCachedGeometry(bool bRecreate, bool bCreateCollision)
{
	const FSimplePolygon* profile = nullptr;
	if (!ensure(DynamicMeshActor.IsValid() && UModumateObjectStatics::GetPolygonProfile(&CachedAssembly, profile)))
	{
		return false;
	}

	// This can be an expected error, if the object is still getting set up before it has a parent assigned.
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (parentObj == nullptr)
	{
		return false;
	}

	LineStartPos = parentObj->GetCorner(0);
	LineEndPos = parentObj->GetCorner(1);
	LineDir = (LineEndPos - LineStartPos).GetSafeNormal();

	UModumateGeometryStatics::FindBasisVectors(LineNormal, LineUp, LineDir);
	if (InstanceData.Rotation != 0.0f)
	{
		FQuat rotation(LineDir, FMath::DegreesToRadians(InstanceData.Rotation));
		LineNormal = rotation * LineNormal;
		LineUp = rotation * LineUp;
	}

	ProfileFlip.Set(InstanceData.FlipSigns.Z, InstanceData.FlipSigns.X);

	if (!UModumateObjectStatics::GetExtrusionProfilePoints(CachedAssembly, InstanceData.Justification, ProfileFlip, CachedProfilePoints, CachedProfileExtents))
	{
		return false;
	}

	DynamicMeshActor->SetupExtrudedPolyGeometry(CachedAssembly, LineStartPos, LineEndPos, LineNormal, LineUp,
		InstanceData.Justification, UpperExtensions, OuterExtensions, InstanceData.FlipSigns, bRecreate, bCreateCollision);

	return true;
}

void AMOIStructureLine::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	OutPerimeters.Reset();

	TArray<FVector> perimeter;
	UModumateObjectStatics::GetExtrusionObjectPoints(CachedAssembly, LineUp, LineNormal, InstanceData.Justification, ProfileFlip, perimeter);

	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{   // In cut-plane lines.
		const Modumate::FModumateLayerType layerType =
			GetObjectType() == EObjectType::OTMullion ? Modumate::FModumateLayerType::kMullionCut : Modumate::FModumateLayerType::kBeamColumnCut;
		UModumateObjectStatics::GetExtrusionCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox,
			perimeter, LineStartPos, LineEndPos, layerType);
	}
	else
	{   // Beyond cut-plane lines.
		const Modumate::FModumateLayerType layerType =
			GetObjectType() == EObjectType::OTMullion ? Modumate::FModumateLayerType::kMullionBeyond : Modumate::FModumateLayerType::kBeamColumnBeyond;
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
					FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
					FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
					ModumateUnitParams::FThickness::Points(bool(clippedLine.Count) ? 0.15f : 0.05f),
					Modumate::FMColor::Gray128);
				ParentPage->Children.Add(line);
				line->SetLayerTypeRecursive(layerType);
			}
		}

	}
}
