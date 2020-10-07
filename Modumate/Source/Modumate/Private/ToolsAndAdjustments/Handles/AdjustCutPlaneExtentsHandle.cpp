#include "ToolsAndAdjustments/Handles/AdjustCutPlaneExtentsHandle.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/CutPlane.h"
#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/DimensionManager.h"
#include "UI/DimensionActor.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

AAdjustCutPlaneExtentsHandle::AAdjustCutPlaneExtentsHandle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool AAdjustCutPlaneExtentsHandle::BeginUse()
{
	if (!Super::BeginUse() || (TargetMOI == nullptr) || (TargetMOI->GetObjectType() != EObjectType::OTCutPlane))
	{
		return false;
	}

	LastValidLocation = FVector(ForceInitToZero);
	LastValidExtents = FVector2D(ForceInitToZero);
	LastSign = 1.0f;

	int32 numCorners = TargetMOI->GetNumCorners();

	FVector targetNormal = TargetMOI->GetNormal();
	if (!targetNormal.IsNormalized() || numCorners == 0)
	{
		return false;
	}
	FPlane PolyPlane = FPlane(TargetMOI->GetCorner(0), targetNormal);

	AnchorLoc = FVector::PointPlaneProject(GetHandlePosition(), PolyPlane);
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector(PolyPlane));

	FMOIStateData targetModifiedState = TargetOriginalState;
	FMOICutPlaneData cutPlaneData;

	if (ensure(targetModifiedState.CustomData.LoadStructData(cutPlaneData)))
	{
		LastValidExtents = cutPlaneData.Extents;
		OriginalExtents = cutPlaneData.Extents;
		LastValidLocation = cutPlaneData.Location;
		OriginalLocation = cutPlaneData.Location;
	}

	return true;
}

bool AAdjustCutPlaneExtentsHandle::UpdateUse()
{
	Super::UpdateUse();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return true;
	}

	FVector hitPoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);

	FVector dp = hitPoint - AnchorLoc;
	FVector direction = GetHandleDirection();
	float proj = dp | direction;
	LastSign = proj > 0 ? 1.0f : -1.0f;

	FVector offset = direction * proj;

	ALineActor* pendingSegment = nullptr;
	if (auto dimensionActor = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID))
	{
		pendingSegment = dimensionActor->GetLineActor();
	}

	if (pendingSegment != nullptr)
	{
		pendingSegment->Point1 = AnchorLoc;
		pendingSegment->Point2 = AnchorLoc + offset;
		pendingSegment->Color = FColor::Black;
		pendingSegment->Thickness = 3.0f;
	}
	else
	{
		return false;
	}

	UpdateTransform(proj);

	ApplyExtents(true);

	return true;
}

void AAdjustCutPlaneExtentsHandle::EndUse()
{
	TargetMOI->EndPreviewOperation_DEPRECATED();

	// Now that we've reverted the target object back to its original state, clean all objects so that
	// deltas can be applied to the original state, and all of its dependent changes.
	GameState->Document.CleanObjects();

	ApplyExtents(false);

	PostEndOrAbort();
}

bool AAdjustCutPlaneExtentsHandle::HandleInputNumber(float number)
{
	FVector direction = GetHandleDirection();
	FVector offset = direction * number * LastSign;

	UpdateTransform(number * LastSign);
	EndUse();

	return true;
}

FVector AAdjustCutPlaneExtentsHandle::GetHandlePosition() const
{
	if (!ensure(TargetMOI && TargetIndex < TargetMOI->GetNumCorners()))
	{
		return FVector::ZeroVector;
	}

	int32 numCorners = TargetMOI->GetNumCorners();

	int32 edgeEndIdx = (TargetIndex + 1) % numCorners;
	return 0.5f * (TargetMOI->GetCorner(TargetIndex) + TargetMOI->GetCorner(edgeEndIdx));
}

bool AAdjustCutPlaneExtentsHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D& OutWidgetSize, FVector2D& OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->GenericArrowStyle;
	OutWidgetSize = FVector2D(16.0f, 16.0f);
	OutMainButtonOffset = FVector2D(16.0f, 0.0f);

	return true;
}

void AAdjustCutPlaneExtentsHandle::UpdateTransform(float Offset)
{
	if (TargetIndex % 2 == 0)
	{
		LastValidExtents.Y = OriginalExtents.Y + Offset;
	}
	else	
	{
		LastValidExtents.X = OriginalExtents.X + Offset;
	}

	// cut plane location is its center
	LastValidLocation = OriginalLocation + GetHandleDirection() * Offset * 0.5f;
}

void AAdjustCutPlaneExtentsHandle::ApplyExtents(bool bIsPreview)
{
	auto delta = MakeShared<FMOIDelta>();

	FMOIStateData targetModifiedState = TargetOriginalState;
	FMOICutPlaneData cutPlaneData;

	if (ensure(targetModifiedState.CustomData.LoadStructData(cutPlaneData)))
	{
		cutPlaneData.Extents = LastValidExtents;
		cutPlaneData.Location = LastValidLocation;
		targetModifiedState.CustomData.SaveStructData<FMOICutPlaneData>(cutPlaneData);
		delta->AddMutationState(TargetMOI, TargetOriginalState, targetModifiedState);

		if (bIsPreview)
		{
			Controller->GetDocument()->ApplyPreviewDeltas({ delta }, Controller->GetWorld());
		}
		else
		{
			Controller->GetDocument()->ApplyDeltas({ delta }, Controller->GetWorld());
		}
	}
}
