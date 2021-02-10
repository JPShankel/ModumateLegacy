#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"

#include "Components/EditableTextBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/DimensionManager.h"
#include "UI/DimensionActor.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

AAdjustPolyEdgeHandle::AAdjustPolyEdgeHandle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PolyPlane(ForceInitToZero)
{
}

bool AAdjustPolyEdgeHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustPolyPointHandle::OnBeginUse"));

	if (!Super::BeginUse() || (TargetMOI == nullptr))
	{
		return false;
	}

	CurrentDirection = GetHandleDirection();

	OriginalPolyPoints.Reset();
	int32 numCorners = TargetMOI->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		OriginalPolyPoints.Add(TargetMOI->GetCorner(i));
	}
	LastValidPolyPoints = OriginalPolyPoints;

	FVector targetNormal = TargetMOI->GetNormal();
	PolyPlane = FPlane(ForceInitToZero);
	if (targetNormal.IsNormalized() && OriginalPolyPoints.Num() > 0)
	{
		PolyPlane = FPlane(OriginalPolyPoints[0], targetNormal);
	}
	// TODO: coming up with a plane of movement when there is only an edge is not ideal
	else if (TargetMOI->GetObjectType() == EObjectType::OTMetaEdge)
	{
		// if the edge is connected to a face, arbitrarily pick one as the plane for the tool
		auto& graph = Controller->GetDocument()->GetVolumeGraph();
		auto edge = graph.FindEdge(TargetMOI->ID);
		if (edge != nullptr && edge->ConnectedFaces.Num() > 0)
		{
			auto& faceConnection = edge->ConnectedFaces[0];
			auto face = graph.FindFace(faceConnection.FaceID);
			if (face != nullptr)
			{
				PolyPlane = face->CachedPlane;
			}
		}
		else
		{	// Fallback when edge is not connected to faces
			FVector direction = TargetMOI->GetCorner(1) - TargetMOI->GetCorner(0);
			direction.Normalize();

			if (FVector::Parallel(direction, FVector::UpVector))
			{	// XZ plane
				PolyPlane = FPlane(OriginalPolyPoints[0], FVector(0.0f, -1.0f, 0.0f));
			}
			else
			{
				FVector v2 = FVector::CrossProduct(direction, FVector::UpVector);
				v2.Normalize();
				FVector normal = FVector::CrossProduct(direction, v2);
				PolyPlane = FPlane(OriginalPolyPoints[0], normal);
			}
		}
	}
	else if (TargetMOI->GetObjectType() == EObjectType::OTSurfaceEdge)
	{
		int32 targetParentID = TargetMOI->GetParentID();
		auto surfaceObj = Controller->GetDocument()->GetObjectById(targetParentID);
		PolyPlane = FPlane(OriginalPolyPoints[0], surfaceObj->GetNormal());
	}

	// not able to find a plane of movement for the tool
	if (FVector(PolyPlane).IsZero())
	{
		return false;
	}

	AnchorLoc = FVector::PointPlaneProject(GetHandlePosition(), PolyPlane);
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector(PolyPlane));

	return true;
}

bool AAdjustPolyEdgeHandle::UpdateUse()
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
		FVector offset = CurrentDirection * (dp | CurrentDirection);

		pendingSegment->Point1 = AnchorLoc;
		pendingSegment->Point2 = AnchorLoc + offset;
		pendingSegment->Color = FColor::Black;
		pendingSegment->Thickness = 3.0f;
	}
	else
	{
		return false;
	}

	UModumateDocument* doc = Controller->GetDocument();
	TMap<int32, FTransform> objectInfo;
	if (GetTransforms(dp, objectInfo))
	{
		FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), true);
	}

	return true;
}

FVector AAdjustPolyEdgeHandle::GetHandlePosition() const
{
	if (!ensure(TargetMOI && TargetIndex < TargetMOI->GetNumCorners()))
	{
		return FVector::ZeroVector;
	}

	FVector averageTargetPos(ForceInitToZero);
	int32 numCorners = TargetMOI->GetNumCorners();

	int32 edgeEndIdx = (TargetIndex + 1) % numCorners;
	averageTargetPos = 0.5f * (TargetMOI->GetCorner(TargetIndex) + TargetMOI->GetCorner(edgeEndIdx));

	// TODO: offset handle position by some amount of the target object's extrusion, if relevant
	return averageTargetPos;
}

bool AAdjustPolyEdgeHandle::HandleInputNumber(float number)
{
	// TODO: reimplement with UModumateGeometryStatics::TranslatePolygonEdge and new dimension string manager
	UModumateDocument* doc = Controller->GetDocument();

	FVector hitPoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
	FVector dp = hitPoint - AnchorLoc;
	
	if ((dp | CurrentDirection) < 0)
	{
		number *= -1;
	}

	TMap<int32, FTransform> objectInfo;
	if (GetTransforms(number * CurrentDirection, objectInfo))
	{
		// Now that we've reverted the target object back to its original state, clean all objects so that
		// deltas can be applied to the original state, and all of its dependent changes.
		GameState->Document->CleanObjects();

		bool bSuccess = FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), false);

		if (bSuccess)
		{
			static const FString eventName(TEXT("PolyEdgeEnteredDimString"));
			UModumateAnalyticsStatics::RecordEventSimple(this, EModumateAnalyticsCategory::Handles, eventName);
		}

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

	return true;
}

bool AAdjustPolyEdgeHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->GenericArrowStyle;
	OutWidgetSize = FVector2D(16.0f, 16.0f);
	OutMainButtonOffset = FVector2D(16.0f, 0.0f);

	return true;
}

bool AAdjustPolyEdgeHandle::GetTransforms(const FVector Offset, TMap<int32, FTransform>& OutTransforms)
{
	UModumateDocument* doc = Controller->GetDocument();
	if (doc == nullptr)
	{
		return false;
	}

	float translation = Offset | CurrentDirection;

	Modumate::EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(TargetMOI->GetObjectType());
	Modumate::EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(TargetMOI->GetObjectType());
	if (graph3DObjType != Modumate::EGraph3DObjectType::None)
	{
		if (TargetMOI->GetObjectType() == EObjectType::OTMetaPlane)
		{
			auto face = doc->GetVolumeGraph().FindFace(TargetMOI->ID);
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
		else if (TargetMOI->GetObjectType() == EObjectType::OTMetaEdge)
		{
			auto edge = doc->GetVolumeGraph().FindEdge(TargetMOI->ID);
			if (!ensure(edge))
			{
				return false;
			}

			int32 id = TargetIndex == 0 ? edge->StartVertexID : edge->EndVertexID;
			OutTransforms.Add(id, FTransform(OriginalPolyPoints[TargetIndex] + Offset));
		}
		else
		{
			return false;
		}
	}
	else if (graph2DObjType != Modumate::EGraphObjectType::None)
	{
		auto surfaceGraph = GameState->Document->FindSurfaceGraphByObjID(TargetMOI->ID);
		auto surfaceObj = GameState->Document->GetObjectById(TargetMOI->GetParentID());
		auto surfaceParent = surfaceObj ? surfaceObj->GetParentObject() : nullptr;

		if (TargetMOI->GetObjectType() == EObjectType::OTSurfacePolygon)
		{
			auto poly = surfaceGraph->FindPolygon(TargetMOI->ID);

			if (!ensure(poly))
			{
				return false;
			}

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
		else if (TargetMOI->GetObjectType() == EObjectType::OTSurfaceEdge)
		{
			auto edge = surfaceGraph->FindEdge(TargetMOI->ID);

			if (!ensure(edge))
			{
				return false;
			}

			int32 id = TargetIndex == 0 ? edge->StartVertexID : edge->EndVertexID;
			OutTransforms.Add(id, FTransform(OriginalPolyPoints[TargetIndex] + Offset));
		}
		else
		{
			return false;
		}
	}
	else
	{ // TODO: non-Graph 3D Objects? (cut-plane handles)
		return false;
	}

	return true;
}
