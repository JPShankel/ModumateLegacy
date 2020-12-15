// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Trim.h"

#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

AMOITrim::AMOITrim()
	: AModumateObjectInstance()
	, TrimStartPos(ForceInitToZero)
	, TrimEndPos(ForceInitToZero)
	, TrimNormal(ForceInitToZero)
	, TrimUp(ForceInitToZero)
	, TrimDir(ForceInitToZero)
	, TrimScale(FVector::OneVector)
	, UpperExtensions(ForceInitToZero)
	, OuterExtensions(ForceInitToZero)
{
}

FVector AMOITrim::GetLocation() const
{
	const AModumateObjectInstance* parentMOI = GetParentObject();
	if (parentMOI)
	{
		return parentMOI->GetLocation();
	}

	return FVector::ZeroVector;
}

FQuat AMOITrim::GetRotation() const
{
	const AModumateObjectInstance *parentMOI = GetParentObject();
	if (parentMOI)
	{
		return parentMOI->GetRotation();
	}

	return FQuat::Identity;
}

FVector AMOITrim::GetNormal() const
{
	return TrimUp;
}

void AMOITrim::SetupAdjustmentHandles(AEditModelPlayerController_CPP* Controller)
{
	MakeHandle<AAdjustInvertHandle>();
}

bool AMOITrim::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		if (!UpdateCachedStructure())
		{
			return false;
		}

		MarkDirty(EObjectDirtyFlags::Mitering);
		return true;
	}
	case EObjectDirtyFlags::Mitering:
	{
		if (!UpdateMitering())
		{
			return false;
		}

		bool bInPreviewMode = IsInPreviewMode();
		bool bRecreateMesh = !bInPreviewMode;
		bool bCreateCollision = !bInPreviewMode;
		return InternalUpdateGeometry(bRecreateMesh, bCreateCollision);
	}
	case EObjectDirtyFlags::Visuals:
		UpdateVisuals();
		return true;
	default:
		return true;
	}
}

void AMOITrim::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const FSimplePolygon* profile = nullptr;
	if (UModumateObjectStatics::GetPolygonProfile(&GetAssembly(), profile))
	{
		FVector2D profileSize = profile->Extents.GetSize();

		// Trims are not centered on their line start/end positions; it is their origin.
		FVector upOffset = (profileSize.X * TrimUp);
		FVector normalOffset = (profileSize.Y * TrimNormal);

		FVector centroid = 0.5f * (TrimStartPos + TrimEndPos + upOffset + normalOffset);
		FVector boxExtents(FVector::Dist(TrimStartPos, TrimEndPos), profileSize.X, profileSize.Y);
		FQuat boxRot = FRotationMatrix::MakeFromYZ(TrimUp, TrimNormal).ToQuat();

		FModumateSnappingView::GetBoundingBoxPointsAndLines(centroid, boxRot, 0.5f * boxExtents, outPoints, outLines);
	}
}

void AMOITrim::SetIsDynamic(bool bIsDynamic)
{
	if (DynamicMeshActor.IsValid())
	{
		DynamicMeshActor->SetIsDynamic(bIsDynamic);
	}
}

bool AMOITrim::GetIsDynamic() const
{
	return DynamicMeshActor.IsValid() && DynamicMeshActor->GetIsDynamic();
}

bool AMOITrim::GetInvertedState(FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOITrimData modifiedTrimData = InstanceData;
	modifiedTrimData.bUpInverted = !modifiedTrimData.bUpInverted;

	return OutState.CustomData.SaveStructData(modifiedTrimData);
}

void AMOITrim::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	TArray<FVector> perimeter;
	UModumateObjectStatics::GetExtrusionPerimeterPoints(this, TrimUp, TrimNormal, perimeter);
	if (bGetFarLines)
	{   // Beyond lines.
		TArray<FEdge> beyondLines = UModumateObjectStatics::GetExtrusionBeyondLinesFromMesh(Plane, perimeter, TrimStartPos, TrimEndPos);

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
				line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kSeparatorBeyondModuleEdges);
			}
		}
	}
	else
	{   // In-plane lines.
	 UModumateObjectStatics::GetExtrusionCutPlaneDraftingLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox,
			perimeter, TrimStartPos, TrimEndPos, Modumate::FModumateLayerType::kSeparatorCutTrim, 0.3f);
	}
}

bool AMOITrim::UpdateCachedStructure()
{
	const FBIMAssemblySpec& trimAssembly = GetAssembly();
	const UModumateDocument* doc = GetDocument();

	const FSimplePolygon* polyProfile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(&trimAssembly, polyProfile))
	{
		return false;
	}

	// Find the parent surface edge MOI to set up mounting.
	// This can fail gracefully, if the object is still getting set up before it has a parent assigned.
	const AModumateObjectInstance* surfaceEdgeMOI = GetParentObject();
	if ((surfaceEdgeMOI == nullptr) || (surfaceEdgeMOI->GetObjectType() != EObjectType::OTSurfaceEdge) || surfaceEdgeMOI->IsDirty(EObjectDirtyFlags::Structure))
	{
		return false;
	}

	const AModumateObjectInstance* surfaceGraphMOI = surfaceEdgeMOI->GetParentObject();
	if ((surfaceGraphMOI == nullptr) || (surfaceGraphMOI->GetObjectType() != EObjectType::OTSurfaceGraph) || surfaceGraphMOI->IsDirty(EObjectDirtyFlags::Structure))
	{
		return false;
	}

	auto surfaceGraph = doc->FindSurfaceGraph(surfaceGraphMOI->ID);
	auto surfaceEdge = surfaceGraph ? surfaceGraph->FindEdge(surfaceEdgeMOI->ID) : nullptr;
	if (!surfaceGraph.IsValid() || (surfaceEdge == nullptr))
	{
		return false;
	}

	// See if the trim should be offset based on its neighboring polygons
	float maxNeighboringThickness = 0.0f;
	const Modumate::FGraph2DPolygon *surfacePolyLeft, *surfacePolyRight;
	if (!surfaceEdge->GetAdjacentInteriorPolygons(surfacePolyLeft, surfacePolyRight))
	{
		return false;
	}

	const AModumateObjectInstance* leftPolyMOI = surfacePolyLeft ? doc->GetObjectById(surfacePolyLeft->ID) : nullptr;
	const AModumateObjectInstance* rightPolyMOI = surfacePolyRight ? doc->GetObjectById(surfacePolyRight->ID) : nullptr;
	for (auto neighborPolyMOI : { leftPolyMOI, rightPolyMOI })
	{
		if (neighborPolyMOI && (neighborPolyMOI->GetObjectType() == EObjectType::OTSurfacePolygon))
		{
			for (auto neighborPolyChild : neighborPolyMOI->GetChildObjects())
			{
				if (neighborPolyChild->GetObjectType() == EObjectType::OTFinish)
				{
					if (neighborPolyChild->IsDirty(EObjectDirtyFlags::Structure))
					{
						return false;
					}

					float neighborFinishThickness = neighborPolyChild->CalculateThickness();
					maxNeighboringThickness = FMath::Max(maxNeighboringThickness, neighborFinishThickness);
				}
			}
		}
	}

	TrimStartPos = surfaceEdgeMOI->GetCorner(0);
	TrimEndPos = surfaceEdgeMOI->GetCorner(1);
	TrimDir = (TrimEndPos - TrimStartPos).GetSafeNormal();
	TrimNormal = surfaceGraphMOI->GetNormal();

	TrimUp = TrimDir ^ TrimNormal;
	if (InstanceData.bUpInverted)
	{
		TrimUp *= -1.0f;
	}

	TrimScale = FVector::OneVector;

	float justification = InstanceData.UpJustification;
	float justificationDist = justification * polyProfile->Extents.GetSize().Y;
	FVector justificationDelta = justificationDist * TrimUp;
	FVector neighborOffsetDelta = maxNeighboringThickness * TrimNormal;

	TrimStartPos += justificationDelta + neighborOffsetDelta;
	TrimEndPos += justificationDelta + neighborOffsetDelta;

	return true;
}

bool AMOITrim::UpdateMitering()
{
	// TODO: miter with neighboring trim, automatically based on graph connectivity and/or based on saved mitering preferences
	UpperExtensions = OuterExtensions = FVector2D::ZeroVector;
	return true;
}

bool AMOITrim::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
{
	const FBIMAssemblySpec& trimAssembly = GetAssembly();

	return DynamicMeshActor->SetupExtrudedPolyGeometry(trimAssembly, TrimStartPos, TrimEndPos,
		TrimNormal, TrimUp, UpperExtensions, OuterExtensions, TrimScale, bRecreate, bCreateCollision);
}
