#include "ToolsAndAdjustments/Handles/AdjustPolyExtrusionHandle.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/DimensionManager.h"
#include "UI/DimensionActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

bool AAdjustPolyExtrusionHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustPolyExtrusionHandle::OnBeginUse"));

	if (!Super::BeginUse())
	{
		return false;
	}

	if (!TargetMOI || (TargetMOI->GetObjectType() != EObjectType::OTCabinet && TargetMOI->GetObjectType() != EObjectType::OTScopeBox))
	{
		UE_LOG(LogCallTrace, Error, TEXT("AdjustPolyExtrusionHandle only currently supported for Cabinets!"));
		return false;
	}

	if (!ensure(TargetMOI->GetNumCorners() > 0))
	{
		return false;
	}

	OriginalPlane = FPlane(TargetMOI->GetCorner(0), TargetMOI->GetNormal());
	OriginalExtrusion = TargetMOI->GetExtents().Y;
	LastValidExtrusion = OriginalExtrusion;

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, TargetMOI->GetNormal());

	return true;
}

bool AAdjustPolyExtrusionHandle::UpdateUse()
{
	Super::UpdateUse();

	float extrusionDelta = 0.f;
	bool snapCursor = false;
	bool snapStructural = false;
	FVector hitPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	switch (Controller->EMPlayerState->SnappedCursor.SnapType)
	{
	case ESnapType::CT_CORNERSNAP:
	case ESnapType::CT_MIDSNAP:
	case ESnapType::CT_EDGESNAP:
	case ESnapType::CT_FACESELECT:
		snapStructural = true;
		extrusionDelta = (hitPoint.Z - AnchorLoc.Z) * Sign;
		break;
	default:
		FVector projectedDeltaPos;
		snapCursor = Controller->EMPlayerState->GetSnapCursorDeltaFromRay(AnchorLoc, OriginalPlane, projectedDeltaPos);
		if (snapCursor)
		{
			extrusionDelta = (projectedDeltaPos - AnchorLoc) | (Sign * OriginalPlane);
		}
	}

	// project pending segment onto extrusion vector
	float newExtrusion = OriginalExtrusion + extrusionDelta;
	ALineActor* pendingSegment = nullptr;
	if (auto dimensionActor = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID))
	{
		pendingSegment = dimensionActor->GetLineActor();
	}

	if (pendingSegment != nullptr)
	{
		pendingSegment->Point2 = AnchorLoc + OriginalPlane * Sign * extrusionDelta;
	}

	if (snapCursor || snapStructural)
	{
		if ((newExtrusion > 0.0f) && (newExtrusion != LastValidExtrusion))
		{
			LastValidExtrusion = newExtrusion;
			ApplyExtrusion(true);
		}
	}

	return true;
}

void AAdjustPolyExtrusionHandle::EndUse()
{
	ApplyExtrusion(false);

	if (!IsActorBeingDestroyed())
	{
		PostEndOrAbort();
	}
}

FVector AAdjustPolyExtrusionHandle::GetHandlePosition() const
{
	int32 numCorners = TargetMOI->GetNumCorners();
	if (!ensure(numCorners > 0))
	{
		return TargetMOI->GetObjectLocation();
	}

	FVector objectNormal = TargetMOI->GetNormal();
	FVector offset = (Sign > 0.0f) ? (TargetMOI->GetExtents().Y * objectNormal) : FVector::ZeroVector;

	// this is assuming that the target object is a prism and GetCorner enumerates the base points first
	int32 numBasePoints = numCorners / 2.0f;

	FVector centroid = FVector::ZeroVector;
	for (int32 cornerIdx = 0; cornerIdx < numBasePoints; cornerIdx++)
	{
		centroid += TargetMOI->GetCorner(cornerIdx);
	}
	
	centroid /= numBasePoints;

	return offset + centroid;
}

FVector AAdjustPolyExtrusionHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? Sign * TargetMOI->GetNormal() : FVector::ZeroVector;
}

bool AAdjustPolyExtrusionHandle::HandleInputNumber(float number)
{
	float dir = FMath::Sign(LastValidExtrusion - OriginalExtrusion);
	LastValidExtrusion = OriginalExtrusion + number * dir;

	EndUse();
	return true;
}

void AAdjustPolyExtrusionHandle::ApplyExtrusion(bool bIsPreview)
{
	FMOIDelta delta;
	auto state = ((const FModumateObjectInstance*)TargetMOI)->GetDataState();
	state.StateType = EMOIDeltaType::Mutate;
	delta.StatePairs.Add(TPair<FMOIStateData, FMOIStateData>(state, state));
	delta.StatePairs[0].Value.Extents.Y = LastValidExtrusion;

	TArray<FDeltaPtr> deltas;
	deltas.Add(MakeShared<FMOIDelta>(delta));

	if (bIsPreview)
	{
		Controller->GetDocument()->ApplyPreviewDeltas(deltas, Controller->GetWorld());
	}
	else
	{
		Controller->GetDocument()->ApplyDeltas(deltas, Controller->GetWorld());
	}
}

bool AAdjustPolyExtrusionHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->GenericArrowStyle;
	OutWidgetSize = FVector2D(16.0f, 16.0f);
	OutMainButtonOffset = FVector2D(16.0f, 0.0f);
	return true;
}
