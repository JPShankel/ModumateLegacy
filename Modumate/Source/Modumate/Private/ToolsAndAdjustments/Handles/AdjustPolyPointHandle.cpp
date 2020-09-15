#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

AAdjustPolyPointHandle::AAdjustPolyPointHandle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAdjustPolyEdge(false)
	, PolyPlane(ForceInitToZero)
{
}

bool AAdjustPolyPointHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustPolyPointHandle::OnBeginUse"));

	if (!Super::BeginUse() || (TargetMOI == nullptr))
	{
		return false;
	}

	OriginalDirection = GetHandleDirection();

	OriginalPolyPoints.Reset();
	int32 numCorners = TargetMOI->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		OriginalPolyPoints.Add(TargetMOI->GetCorner(i));
	}
	LastValidPolyPoints = OriginalPolyPoints;

	FVector targetNormal = TargetMOI->GetNormal();
	if (!targetNormal.IsNormalized() || (OriginalPolyPoints.Num() == 0))
	{
		return false;
	}

	PolyPlane = FPlane(OriginalPolyPoints[0], targetNormal);
	AnchorLoc = FVector::PointPlaneProject(GetHandlePosition(), PolyPlane);
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector(PolyPlane));

	if (auto world = GetWorld())
	{
		GameInstance = Cast<UModumateGameInstance>(GetWorld()->GetGameInstance());

		PendingSegmentID = GameInstance->DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass())->ID;
	}

	return true;
}

bool AAdjustPolyPointHandle::UpdateUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustPolyPointHandle::OnUpdateUse"));

	Super::UpdateUse();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return true;
	}

	FVector hitPoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);

	FVector dp = hitPoint - AnchorLoc;

	if (!FMath::IsNearlyEqual(dp.Z, 0.0f, 0.01f))
	{
		FAffordanceLine affordance;
		affordance.Color = FLinearColor::Blue;
		affordance.EndPoint = hitPoint;
		affordance.StartPoint = FVector(hitPoint.X, hitPoint.Y, AnchorLoc.Z);
		affordance.Interval = 4.0f;
		Controller->EMPlayerState->AffordanceLines.Add(affordance);
	}

	ALineActor *pendingSegment = nullptr;
	if (auto dimensionActor = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID))
	{
		pendingSegment = dimensionActor->GetLineActor();
	}
	if (pendingSegment != nullptr)
	{
		pendingSegment->Point1 = AnchorLoc;
		pendingSegment->Point2 = AnchorLoc + OriginalDirection * (dp | OriginalDirection);
		pendingSegment->Color = FColor::Black;
		pendingSegment->Thickness = 3.0f;
	}
	else
	{
		return false;
	}


	FModumateDocument* doc = Controller->GetDocument();
	if (doc != nullptr)
	{
		TMap<int32, FTransform> objectInfo;
		
		bool bMetaPlaneTarget = (TargetMOI->GetObjectType() == EObjectType::OTMetaPlane);
		bool bSurfacePolyTarget = (TargetMOI->GetObjectType() == EObjectType::OTSurfacePolygon);
		if (bMetaPlaneTarget)
		{
			auto face = doc->GetVolumeGraph().FindFace(TargetMOI->ID);
			if (bAdjustPolyEdge)
			{
				int32 numPolyPoints = OriginalPolyPoints.Num();
				int32 edgeStartIdx = TargetIndex;
				int32 edgeEndIdx = (TargetIndex + 1) % numPolyPoints;

				float translation = (dp | OriginalDirection);
				FVector edgeStartPoint, edgeEndPoint;
				if (UModumateGeometryStatics::TranslatePolygonEdge(OriginalPolyPoints, FVector(PolyPlane), edgeStartIdx, translation, edgeStartPoint, edgeEndPoint))
				{
					objectInfo.Add(face->VertexIDs[edgeStartIdx], FTransform(edgeStartPoint));
					objectInfo.Add(face->VertexIDs[edgeEndIdx], FTransform(edgeEndPoint));
				}
			}
			else
			{
				objectInfo.Add(face->VertexIDs[TargetIndex], FTransform(OriginalPolyPoints[TargetIndex] + dp));
			}
		}
		else if (bSurfacePolyTarget)
		{
			auto surfaceGraph = GameState->Document.FindSurfaceGraphByObjID(TargetMOI->ID);
			auto surfaceObj = GameState->Document.GetObjectById(TargetMOI->GetParentID());
			auto surfaceParent = surfaceObj ? surfaceObj->GetParentObject() : nullptr;
			auto poly = surfaceGraph->FindPolygon(TargetMOI->ID);

			if (!ensure(poly))
			{
				return false;
			}

			if (bAdjustPolyEdge)
			{
				int32 numPolyPoints = OriginalPolyPoints.Num();
				int32 edgeStartIdx = TargetIndex;
				int32 edgeEndIdx = (TargetIndex + 1) % numPolyPoints;

				float translation = (dp | OriginalDirection);

				FVector edgeStartPoint, edgeEndPoint;
				if (UModumateGeometryStatics::TranslatePolygonEdge(OriginalPolyPoints, FVector(PolyPlane), edgeStartIdx, translation, edgeStartPoint, edgeEndPoint))
				{
					objectInfo.Add(poly->CachedPerimeterVertexIDs[edgeStartIdx], FTransform(edgeStartPoint));
					objectInfo.Add(poly->CachedPerimeterVertexIDs[edgeEndIdx], FTransform(edgeEndPoint));
				}
			}
			else
			{
				objectInfo.Add(poly->CachedPerimeterVertexIDs[TargetIndex], FTransform(OriginalPolyPoints[TargetIndex] + dp));
			}
		}
		else
		{ // TODO: non-Graph 3D Objects?
			return false;
		}

		FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), true);
	}

	return true;
}

void AAdjustPolyPointHandle::PostEndOrAbort()
{
	if (GameInstance && GameInstance->DimensionManager)
	{
		GameInstance->DimensionManager->ReleaseDimensionActor(PendingSegmentID);
		PendingSegmentID = MOD_ID_NONE;
	}

	Super::PostEndOrAbort();
}

FVector AAdjustPolyPointHandle::GetHandlePosition() const
{
	if (!ensure(TargetMOI && TargetIndex < TargetMOI->GetNumCorners()))
	{
		return FVector::ZeroVector;
	}

	FVector averageTargetPos(ForceInitToZero);
	int32 numCorners = TargetMOI->GetNumCorners();

	if (bAdjustPolyEdge)
	{
		int32 edgeEndIdx = (TargetIndex + 1) % numCorners;
		averageTargetPos = 0.5f * (TargetMOI->GetCorner(TargetIndex) + TargetMOI->GetCorner(edgeEndIdx));
	}
	else
	{
		averageTargetPos = TargetMOI->GetCorner(TargetIndex);
	}

	switch (TargetMOI->GetObjectType())
	{
		// TODO: this is an awkward stand-in for the fact that these all inherit from FMOIPlaneImplBase, this should be cleaner
	case EObjectType::OTMetaPlane:
	case EObjectType::OTSurfacePolygon:
	case EObjectType::OTCutPlane:
		return averageTargetPos;
	case EObjectType::OTScopeBox:
		return TargetMOI->GetObjectRotation().RotateVector(TargetMOI->GetNormal() * 0.5f * TargetMOI->GetExtents().Y + averageTargetPos);
	default:
		return TargetMOI->GetObjectRotation().RotateVector(FVector(0, 0, 0.5f * TargetMOI->GetExtents().Y) + averageTargetPos);
	}
}

FVector AAdjustPolyPointHandle::GetHandleDirection() const
{
	if (!ensure(TargetMOI))
	{
		return FVector::ZeroVector;
	}

	if (!bAdjustPolyEdge)
	{
		return FVector::ZeroVector;
	}

	int32 numPoints = TargetMOI->GetNumCorners();
	TArray<FVector> targetPoints;
	for (int32 i = 0; i < numPoints; ++i)
	{
		targetPoints.Add(TargetMOI->GetCorner(i));
	}

	// This would be redundant, but the handles need to recalculate the plane in order to determine
	// how the points wind, which may not result in the same plane normal as the reported object normal.
	FPlane targetPlane;
	if (!ensure(UModumateGeometryStatics::GetPlaneFromPoints(targetPoints, targetPlane)))
	{
		return FVector::ZeroVector;
	}

	FVector targetNormal(targetPlane);

	int32 numPolyPoints = targetPoints.Num();
	int32 edgeStartIdx = TargetIndex;
	int32 edgeEndIdx = (TargetIndex + 1) % numPolyPoints;

	const FVector &edgeStartPoint = targetPoints[edgeStartIdx];
	const FVector &edgeEndPoint = targetPoints[edgeEndIdx];
	FVector edgeDir = (edgeEndPoint - edgeStartPoint).GetSafeNormal();
	FVector edgeNormal = (edgeDir ^ targetNormal);

	return edgeNormal;
}

bool AAdjustPolyPointHandle::HandleInputNumber(float number)
{
	// TODO: reimplement with UModumateGeometryStatics::TranslatePolygonEdge and new dimension string manager
	return false;
}

void AAdjustPolyPointHandle::SetAdjustPolyEdge(bool bInAdjustPolyEdge)
{
	bAdjustPolyEdge = bInAdjustPolyEdge;
}

bool AAdjustPolyPointHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	if (bAdjustPolyEdge)
	{
		OutButtonStyle = PlayerHUD->HandleAssets->GenericArrowStyle;
		OutWidgetSize = FVector2D(16.0f, 16.0f);
		OutMainButtonOffset = FVector2D(16.0f, 0.0f);
	}
	else
	{
		OutButtonStyle = PlayerHUD->HandleAssets->GenericPointStyle;
		OutWidgetSize = FVector2D(12.0f, 12.0f);
	}

	return true;
}
