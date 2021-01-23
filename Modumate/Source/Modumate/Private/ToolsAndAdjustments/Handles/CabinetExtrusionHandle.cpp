// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/CabinetExtrusionHandle.h"

#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/Cabinet.h"
#include "ToolsAndAdjustments/Handles/CabinetFrontFaceHandle.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/DimensionActor.h"
#include "UI/DimensionManager.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"


bool ACabinetExtrusionHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("ACabinetExtrusionHandle::OnBeginUse"));

	if (!Super::BeginUse())
	{
		return false;
	}

	if (!TargetMOI || (TargetMOI->GetObjectType() != EObjectType::OTCabinet))
	{
		UE_LOG(LogCallTrace, Error, TEXT("ACabinetExtrusionHandle only currently supported for Cabinets!"));
		return false;
	}

	if (!ensure(TargetMOI->GetNumCorners() > 0))
	{
		return false;
	}

	FMOICabinetData cabinetData;
	if (!TargetOriginalState.CustomData.LoadStructData(cabinetData))
	{
		return false;
	}

	OriginalPlane = FPlane(TargetMOI->GetCorner(0), TargetMOI->GetNormal());
	OriginalExtrusion = cabinetData.ExtrusionDist;
	LastValidExtrusion = OriginalExtrusion;

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, TargetMOI->GetNormal());

	return true;
}

bool ACabinetExtrusionHandle::UpdateUse()
{
	Super::UpdateUse();

	float extrusionDelta = 0.f;
	bool snapCursor = false;
	bool snapStructural = false;
	FVector hitPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	switch (Controller->EMPlayerState->SnappedCursor.SnapType)
	{
	case ESnapType::CT_NOSNAP:
	{
		FVector projectedDeltaPos;
		snapCursor = Controller->EMPlayerState->GetSnapCursorDeltaFromRay(AnchorLoc, OriginalPlane, projectedDeltaPos);
		if (snapCursor)
		{
			extrusionDelta = (projectedDeltaPos - AnchorLoc) | (Sign * OriginalPlane);
		}
	}
	break;
	default:
		snapStructural = true;
		extrusionDelta = (hitPoint - AnchorLoc) | FVector(OriginalPlane);
		break;
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
		if (newExtrusion > 0.0f)
		{
			LastValidExtrusion = newExtrusion;
			ApplyExtrusion(true);
		}
	}

	return true;
}

void ACabinetExtrusionHandle::EndUse()
{
	ApplyExtrusion(false);

	if (!IsActorBeingDestroyed())
	{
		PostEndOrAbort();
	}
}

FVector ACabinetExtrusionHandle::GetHandlePosition() const
{
	// this is assuming that GetNumCorners only counts the base points
	int32 numBasePoints = TargetMOI->GetNumCorners();

	FMOICabinetData cabinetData;
	if (!ensure((numBasePoints > 0) && TargetMOI->GetStateData().CustomData.LoadStructData(cabinetData)))
	{
		return TargetMOI->GetLocation();
	}

	FVector objectNormal = TargetMOI->GetNormal();
	FVector offset = (Sign > 0.0f) ? (cabinetData.ExtrusionDist * objectNormal) : FVector::ZeroVector;

	FVector centroid = FVector::ZeroVector;
	for (int32 cornerIdx = 0; cornerIdx < numBasePoints; cornerIdx++)
	{
		centroid += TargetMOI->GetCorner(cornerIdx);
	}
	centroid /= numBasePoints;

	FVector baseAxisX, baseAxisY;
	UModumateGeometryStatics::FindBasisVectors(baseAxisX, baseAxisY, objectNormal);

	FVector centerOffset = -ACabinetFrontFaceHandle::BaseCenterOffset * baseAxisY;

	return offset + centroid + centerOffset;
}

FVector ACabinetExtrusionHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? Sign * TargetMOI->GetNormal() : FVector::ZeroVector;
}

bool ACabinetExtrusionHandle::HandleInputNumber(float number)
{
	float dir = FMath::Sign(LastValidExtrusion - OriginalExtrusion);
	LastValidExtrusion = OriginalExtrusion + number * dir;

	EndUse();
	return true;
}

void ACabinetExtrusionHandle::ApplyExtrusion(bool bIsPreview)
{
	auto delta = MakeShared<FMOIDelta>();

	FMOIStateData targetModifiedState = TargetOriginalState;
	FMOICabinetData modifiedCabinetData;
	if (ensure(targetModifiedState.CustomData.LoadStructData(modifiedCabinetData)))
	{
		modifiedCabinetData.ExtrusionDist = LastValidExtrusion;
		targetModifiedState.CustomData.SaveStructData(modifiedCabinetData);

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

bool ACabinetExtrusionHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D& OutWidgetSize, FVector2D& OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->GenericArrowStyle;
	OutWidgetSize = FVector2D(16.0f, 16.0f);
	OutMainButtonOffset = FVector2D(16.0f, 0.0f);
	return true;
}
