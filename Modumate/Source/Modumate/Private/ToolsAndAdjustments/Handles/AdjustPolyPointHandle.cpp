#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"

#include "Components/EditableTextBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/DimensionManager.h"
#include "UI/DimensionActor.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/DimensionWidget.h"
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

		auto dimensionActor = GameInstance->DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
		PendingSegmentID = dimensionActor->ID;

		auto dimensionWidget = dimensionActor->DimensionText;
		dimensionWidget->Measurement->SetIsReadOnly(false);
		dimensionWidget->Measurement->OnTextCommitted.AddDynamic(this, &AAdjustPolyPointHandle::OnTextCommitted);

		GameInstance->DimensionManager->SetActiveActorID(PendingSegmentID);
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

	auto dimensionWidget = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->DimensionText;
	if (dimensionWidget && dimensionWidget->Measurement->HasAnyUserFocus())
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

	ALineActor* pendingSegment = nullptr;
	if (auto dimensionActor = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID))
	{
		pendingSegment = dimensionActor->GetLineActor();
	}

	if (pendingSegment != nullptr)
	{
		FVector offset = bAdjustPolyEdge ? OriginalDirection * (dp | OriginalDirection) : dp;

		pendingSegment->Point1 = AnchorLoc;
		pendingSegment->Point2 = AnchorLoc + offset;
		pendingSegment->Color = FColor::Black;
		pendingSegment->Thickness = 3.0f;
	}
	else
	{
		return false;
	}


	FModumateDocument* doc = Controller->GetDocument();
	TMap<int32, FTransform> objectInfo;
	if (GetTransforms(dp, objectInfo))
	{
		FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), true);
	}

	return true;
}

void AAdjustPolyPointHandle::PostEndOrAbort()
{
	if (GameInstance && GameInstance->DimensionManager)
	{
		auto dimensionWidget = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->DimensionText;
		dimensionWidget->Measurement->OnTextCommitted.RemoveDynamic(this, &AAdjustPolyPointHandle::OnTextCommitted);

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

void AAdjustPolyPointHandle::OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	auto dimension = UModumateDimensionStatics::StringToFormattedDimension(Text.ToString());

	float lengthValue = dimension.Centimeters;

	auto dimensionWidget = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->DimensionText;
	dimensionWidget->UpdateText(lengthValue);

	FModumateDocument* doc = Controller->GetDocument();
	TMap<int32, FTransform> objectInfo;
	if (GetTransforms(lengthValue * OriginalDirection, objectInfo))
	{
		// TODO: preview operation is no longer necessary, but removing this could cause ensures
		// until the other handles are refactored
		TargetMOI->EndPreviewOperation();

		// Now that we've reverted the target object back to its original state, clean all objects so that
		// deltas can be applied to the original state, and all of its dependent changes.
		GameState->Document.CleanObjects();

		FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), false);

		// TODO: the deltas should be an outparam of EndUse (and EndUse would need to be refactored)
		if (!IsActorBeingDestroyed())
		{
			PostEndOrAbort();
		}
	}
	else
	{
		EndUse();
	}
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

bool AAdjustPolyPointHandle::GetTransforms(const FVector Offset, TMap<int32, FTransform>& OutTransforms)
{
	FModumateDocument* doc = Controller->GetDocument();
	if (doc == nullptr)
	{
		return false;
	}

	float translation = Offset | OriginalDirection;

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

			FVector edgeStartPoint, edgeEndPoint;
			if (UModumateGeometryStatics::TranslatePolygonEdge(OriginalPolyPoints, FVector(PolyPlane), edgeStartIdx, translation, edgeStartPoint, edgeEndPoint))
			{
				OutTransforms.Add(face->VertexIDs[edgeStartIdx], FTransform(edgeStartPoint));
				OutTransforms.Add(face->VertexIDs[edgeEndIdx], FTransform(edgeEndPoint));
			}
		}
		else
		{
			OutTransforms.Add(face->VertexIDs[TargetIndex], FTransform(OriginalPolyPoints[TargetIndex] + Offset));
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

			FVector edgeStartPoint, edgeEndPoint;
			if (UModumateGeometryStatics::TranslatePolygonEdge(OriginalPolyPoints, FVector(PolyPlane), edgeStartIdx, translation, edgeStartPoint, edgeEndPoint))
			{
				OutTransforms.Add(poly->CachedPerimeterVertexIDs[edgeStartIdx], FTransform(edgeStartPoint));
				OutTransforms.Add(poly->CachedPerimeterVertexIDs[edgeEndIdx], FTransform(edgeEndPoint));
			}
		}
		else
		{
			OutTransforms.Add(poly->CachedPerimeterVertexIDs[TargetIndex], FTransform(OriginalPolyPoints[TargetIndex] + Offset));
		}
	}
	else
	{ // TODO: non-Graph 3D Objects? (cut-plane handles)
		return false;
	}

	return true;
}
