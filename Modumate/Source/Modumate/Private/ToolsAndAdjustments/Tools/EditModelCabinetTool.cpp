// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCabinetTool.h"

#include "Algo/Transform.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Objects/Cabinet.h"
#include "Graph/Graph2D.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"



UCabinetTool::UCabinetTool()
	: Super()
	, TargetPolygonID(MOD_ID_NONE)
	, BaseOrigin(ForceInitToZero)
	, BaseNormal(ForceInitToZero)
	, ExtrusionDist(0.0f)
	, PrevMouseMode(EMouseMode::Object)
{
}

bool UCabinetTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	Controller->DeselectAll();
	PrevMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;

	return true;
}

bool UCabinetTool::HandleInputNumber(double n)
{
	if (!IsInUse() || (n < MinimumExtrusionDist))
	{
		return false;
	}

	ExtrusionDist = n;

	if (!EnterNextStage())
	{
		EndUse();
	}

	return true;
}

bool UCabinetTool::Deactivate()
{
	Controller->EMPlayerState->SnappedCursor.MouseMode = PrevMouseMode;

	return Super::Deactivate();
}

bool UCabinetTool::BeginUse()
{
	int32 numBasePoints = BasePoints.Num();
	if ((TargetPolygonID == MOD_ID_NONE) || (numBasePoints < 3) || !BaseNormal.IsNormalized() || !Super::BeginUse())
	{
		return false;
	}

	auto& cursor = Controller->EMPlayerState->SnappedCursor;
	FPlane basePlane(BaseOrigin, BaseNormal);

	// Correct the cursor affordance to start from the polygon's origin along its plane
	BaseOrigin = FVector::PointPlaneProject(cursor.WorldPosition, basePlane);
	cursor.SetAffordanceFrame(BaseOrigin, BaseNormal, FVector::ZeroVector, true, true);

	ExtrusionDist = 0.0f;
	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);

	if (dimensionActor != nullptr)
	{
		auto pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = BaseOrigin;
		pendingSegment->Point2 = BaseOrigin;
		pendingSegment->Color = ExtrusionLineColor;
		pendingSegment->Thickness = ExtrusionLineThickness;
	}

	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	return true;
}

bool UCabinetTool::FrameUpdate()
{
	Super::FrameUpdate();

	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
	if (!cursor.bValid)
	{
		return false;
	}

	auto& affordanceLines = Controller->EMPlayerState->AffordanceLines;

	// We're choosing an extrusion along the normal of the base for the cabinet
	if (IsInUse())
	{
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		auto dimensionLine = dimensionActor ? dimensionActor->GetLineActor() : nullptr;
		if (dimensionLine == nullptr)
		{
			return false;
		}

		float cursorExtrusion = (cursor.WorldPosition - BaseOrigin) | BaseNormal;
		if (cursorExtrusion < MinimumExtrusionDist)
		{
			FVector extrusionIntersection, cursorIntersection;
			float distBetweenRays;
			if (UModumateGeometryStatics::FindShortestDistanceBetweenRays(BaseOrigin, BaseNormal, cursor.OriginPosition, cursor.OriginDirection, extrusionIntersection, cursorIntersection, distBetweenRays))
			{
				cursorExtrusion = (extrusionIntersection - BaseOrigin) | BaseNormal;
			}
		}

		if (cursorExtrusion < MinimumExtrusionDist)
		{
			ExtrusionDist = 0.0f;
		}
		else
		{
			ExtrusionDist = cursorExtrusion;
			FVector extrusionDelta = ExtrusionDist * BaseNormal;

			int32 numBasePoints = BasePoints.Num();
			for (int32 baseIdxStart = 0; baseIdxStart < numBasePoints; ++baseIdxStart)
			{
				int32 baseIdxEnd = (baseIdxStart + 1) % numBasePoints;
				const FVector& baseEdgeStart = BasePoints[baseIdxStart];
				const FVector& baseEdgeEnd = BasePoints[baseIdxEnd];
				FVector topEdgeStart = baseEdgeStart + extrusionDelta;
				FVector topEdgeEnd = baseEdgeEnd + extrusionDelta;

				affordanceLines.Add(FAffordanceLine(baseEdgeStart, baseEdgeEnd, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness));
				affordanceLines.Add(FAffordanceLine(baseEdgeStart, topEdgeStart, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness));
				affordanceLines.Add(FAffordanceLine(topEdgeStart, topEdgeEnd, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness));
			}

			dimensionLine->Point1 = BaseOrigin;
			dimensionLine->Point2 = BaseOrigin + extrusionDelta;
		}
	}
	// We're choosing a polygon to host a cabinet to
	else
	{
		TargetPolygonID = MOD_ID_NONE;
		BasePoints.Reset();

		auto targetMOI = GameState->Document->ObjectFromActor(cursor.Actor);
		if (targetMOI && (targetMOI->GetObjectType() == EObjectType::OTSurfacePolygon) && IsObjectInActiveGroup(targetMOI))
		{
			auto surfaceGraph = GameState->Document->FindSurfaceGraphByObjID(targetMOI->ID);
			if (!surfaceGraph.IsValid())
			{
				return false;
			}

			// TODO: we disallow polygons with holes here, but we should either officially support it,
			// or have a more centralized way of validating which polygons that cabinets can be hosted to,
			// and removing cabinets from polygons that are no longer valid.
			auto surfacePoly = surfaceGraph->FindPolygon(targetMOI->ID);
			if ((surfacePoly == nullptr) || (surfacePoly->ContainedPolyIDs.Num() > 0))
			{
				return false;
			}

			int32 existingCabinetID = MOD_ID_NONE;
			auto targetChildren = targetMOI->GetChildObjects();
			for (auto* targetChild : targetChildren)
			{
				if (targetChild && (targetChild->GetObjectType() == EObjectType::OTCabinet))
				{
					existingCabinetID = targetChild->ID;
					break;
				}
			}

			// TODO: allow replacing existing cabinet's assembly, or delete an existing cabinet entirely
			if (existingCabinetID == MOD_ID_NONE)
			{
				TargetPolygonID = targetMOI->ID;

				int32 numPoints = targetMOI->GetNumCorners();
				for (int32 cornerIdx = 0; cornerIdx < numPoints; ++cornerIdx)
				{
					BasePoints.Add(targetMOI->GetCorner(cornerIdx));
				}

				BaseOrigin = targetMOI->GetLocation();
				BaseNormal = targetMOI->GetNormal();
			}
		}

		// Show affordance lines for outline of current surface polygon target
		if (TargetPolygonID != MOD_ID_NONE)
		{
			int32 numBasePoints = BasePoints.Num();
			for (int32 baseIdxStart = 0; baseIdxStart < numBasePoints; ++baseIdxStart)
			{
				int32 baseIdxEnd = (baseIdxStart + 1) % numBasePoints;
				const FVector& basePointStart = BasePoints[baseIdxStart];
				const FVector& basePointEnd = BasePoints[baseIdxEnd];

				affordanceLines.Add(FAffordanceLine(basePointStart, basePointEnd,
					AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
				);
			}
		}
	}

	return true;
}

bool UCabinetTool::AbortUse()
{
	if (DimensionManager)
	{
		DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	}

	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;

	return Super::AbortUse();
}

bool UCabinetTool::EndUse()
{
	if (DimensionManager)
	{
		DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	}

	return Super::EndUse();
}

bool UCabinetTool::EnterNextStage()
{
	if ((TargetPolygonID == MOD_ID_NONE) || (BasePoints.Num() < 3) || !BaseNormal.IsNormalized() || (ExtrusionDist < MinimumExtrusionDist))
	{
		return true;
	}

	FMOICabinetData newCabinetData;
	newCabinetData.ExtrusionDist = ExtrusionDist;

	FMOIStateData stateData;
	stateData.ID = GameState->Document->GetNextAvailableID();
	stateData.ObjectType = EObjectType::OTCabinet;
	stateData.ParentID = TargetPolygonID;
	stateData.AssemblyGUID = AssemblyGUID;
	stateData.CustomData.SaveStructData(newCabinetData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	bool bAppliedDeltas = GameState->Document->ApplyDeltas({ delta }, GetWorld());

	// If we succeeded, then select the new cabinet object (and only that object)
	AModumateObjectInstance *cabinetObj = bAppliedDeltas ? GameState->Document->GetObjectById(stateData.ID) : nullptr;
	if (cabinetObj)
	{
		Controller->SetObjectSelected(cabinetObj, true, true);
		cabinetObj->ShowAdjustmentHandles(Controller, true);
	}

	return false;
}
