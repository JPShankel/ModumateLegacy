// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Trim.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"
#include "Drafting/ModumateDraftingElements.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

#include "Algo/ForEach.h"

FMOITrimImpl::FMOITrimImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
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

FMOITrimImpl::~FMOITrimImpl()
{
}

FVector FMOITrimImpl::GetLocation() const
{
	const FModumateObjectInstance* parentMOI = MOI ? MOI->GetParentObject() : nullptr;
	if (parentMOI)
	{
		return parentMOI->GetObjectLocation();
	}

	return FVector::ZeroVector;
}

FQuat FMOITrimImpl::GetRotation() const
{
	const FModumateObjectInstance *parentMOI = MOI ? MOI->GetParentObject() : nullptr;
	if (parentMOI)
	{
		return parentMOI->GetObjectRotation();
	}

	return FQuat::Identity;
}

FVector FMOITrimImpl::GetNormal() const
{
	return TrimUp;
}

void FMOITrimImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
}

void FMOITrimImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* Controller)
{
	MOI->MakeHandle<AAdjustInvertHandle>();
}

void FMOITrimImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP* Controller, bool bShow)
{
	return FModumateObjectInstanceImplBase::ShowAdjustmentHandles(Controller, bShow);
}

bool FMOITrimImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		if (!UpdateCachedStructure())
		{
			return false;
		}

		MOI->MarkDirty(EObjectDirtyFlags::Mitering);
		return true;
	}
	case EObjectDirtyFlags::Mitering:
	{
		if (!UpdateMitering())
		{
			return false;
		}

		bool bInPreviewMode = MOI->GetIsInPreviewMode();
		bool bRecreateMesh = !bInPreviewMode;
		bool bCreateCollision = !bInPreviewMode;
		return InternalUpdateGeometry(bRecreateMesh, bCreateCollision);
	}
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		return true;
	default:
		return true;
	}
}

void FMOITrimImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const FSimplePolygon* profile = nullptr;
	if (UModumateObjectStatics::GetPolygonProfile(&MOI->GetAssembly(), profile))
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

void FMOITrimImpl::SetIsDynamic(bool bIsDynamic)
{
	if (DynamicMeshActor.IsValid())
	{
		DynamicMeshActor->SetIsDynamic(bIsDynamic);
	}
}

bool FMOITrimImpl::GetIsDynamic() const
{
	return DynamicMeshActor.IsValid() && DynamicMeshActor->GetIsDynamic();
}

bool FMOITrimImpl::GetInvertedState(FMOIStateData& OutState) const
{
	OutState = MOI->GetStateData();

	FMOITrimData modifiedTrimData = InstanceData;
	modifiedTrimData.bUpInverted = !modifiedTrimData.bUpInverted;

	return OutState.CustomData.SaveStructData(modifiedTrimData);
}

void FMOITrimImpl::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	const bool bGetFarLines = ParentPage->lineClipping.IsValid();
	TArray<FVector> perimeter;
	UModumateObjectStatics::GetExtrusionPerimeterPoints(MOI, TrimUp, TrimNormal, perimeter);
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

bool FMOITrimImpl::UpdateCachedStructure()
{
	const FBIMAssemblySpec& trimAssembly = MOI->GetAssembly();
	const FModumateDocument* doc = MOI->GetDocument();

	const FSimplePolygon* polyProfile = nullptr;
	if (!UModumateObjectStatics::GetPolygonProfile(&trimAssembly, polyProfile))
	{
		return false;
	}

	// Find the parent surface edge MOI to set up mounting.
	// This can fail gracefully, if the object is still getting set up before it has a parent assigned.
	const FModumateObjectInstance* surfaceEdgeMOI = MOI->GetParentObject();
	if ((surfaceEdgeMOI == nullptr) || (surfaceEdgeMOI->GetObjectType() != EObjectType::OTSurfaceEdge) || surfaceEdgeMOI->IsDirty(EObjectDirtyFlags::Structure))
	{
		return false;
	}

	const FModumateObjectInstance* surfaceGraphMOI = surfaceEdgeMOI->GetParentObject();
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

	const FModumateObjectInstance* leftPolyMOI = surfacePolyLeft ? doc->GetObjectById(surfacePolyLeft->ID) : nullptr;
	const FModumateObjectInstance* rightPolyMOI = surfacePolyRight ? doc->GetObjectById(surfacePolyRight->ID) : nullptr;
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

bool FMOITrimImpl::UpdateMitering()
{
	// TODO: miter with neighboring trim, automatically based on graph connectivity and/or based on saved mitering preferences
	UpperExtensions = OuterExtensions = FVector2D::ZeroVector;
	return true;
}

bool FMOITrimImpl::InternalUpdateGeometry(bool bRecreate, bool bCreateCollision)
{
	const FBIMAssemblySpec& trimAssembly = MOI->GetAssembly();

	return DynamicMeshActor->SetupExtrudedPolyGeometry(trimAssembly, TrimStartPos, TrimEndPos,
		TrimNormal, TrimUp, UpperExtensions, OuterExtensions, TrimScale, bRecreate, bCreateCollision);
}
