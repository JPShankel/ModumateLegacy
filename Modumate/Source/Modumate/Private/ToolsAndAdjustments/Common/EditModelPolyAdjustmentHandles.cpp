// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/EditModelPolyAdjustmentHandles.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Algo/Accumulate.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/AdjustmentHandleWidget.h"
#include "UI/EditModelPlayerHUD.h"


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
	OriginalPolyPoints = TargetMOI->GetControlPoints();
	LastValidPolyPoints = OriginalPolyPoints;

	FVector targetNormal = TargetMOI->GetNormal();
	if (!targetNormal.IsNormalized() || (OriginalPolyPoints.Num() == 0))
	{
		return false;
	}

	PolyPlane = FPlane(OriginalPolyPoints[0], targetNormal);
	AnchorLoc = FVector::PointPlaneProject(GetHandlePosition(), PolyPlane);
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector(PolyPlane));

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

	if (bAdjustPolyEdge)
	{
		int32 numPolyPoints = OriginalPolyPoints.Num();
		int32 edgeStartIdx = TargetIndex;
		int32 edgeEndIdx = (TargetIndex + 1) % numPolyPoints;

		float translation = (dp | OriginalDirection);
		FVector edgeStartPoint, edgeEndPoint;
		if (UModumateGeometryStatics::TranslatePolygonEdge(OriginalPolyPoints, FVector(PolyPlane), edgeStartIdx, translation, edgeStartPoint, edgeEndPoint))
		{
			TargetMOI->SetControlPoint(edgeStartIdx, edgeStartPoint);
			TargetMOI->SetControlPoint(edgeEndIdx, edgeEndPoint);
		}
	}
	else
	{
		TargetMOI->SetControlPoint(TargetIndex, OriginalPolyPoints[TargetIndex] + dp);
	}

	const TArray<FVector> &newPolyPoints = TargetMOI->GetControlPoints();
	if (UModumateGeometryStatics::ArePolygonEdgesValid(newPolyPoints, PolyPlane))
	{
		LastValidPolyPoints = newPolyPoints;
		UpdateTargetGeometry();
	}
	else
	{
		TargetMOI->SetControlPoints(LastValidPolyPoints);
	}

	return true;
}

FVector AAdjustPolyPointHandle::GetHandlePosition() const
{
	if (!ensure(TargetMOI))
	{
		return FVector::ZeroVector;
	}

	FVector averageTargetPos(ForceInitToZero);
	const TArray<FVector> &controlPoints = TargetMOI->GetControlPoints();
	int32 numPoints = controlPoints.Num();

	if (bAdjustPolyEdge)
	{
		int32 edgeEndIdx = (TargetIndex + 1) % numPoints;
		averageTargetPos = 0.5f * (controlPoints[TargetIndex] + controlPoints[edgeEndIdx]);
	}
	else
	{
		averageTargetPos = controlPoints[TargetIndex];
	}

	if (TargetMOI->GetObjectType() == EObjectType::OTMetaPlane || TargetMOI->GetObjectType() == EObjectType::OTCutPlane)
	{
		return averageTargetPos;
	}
	else if (TargetMOI->GetObjectType() == EObjectType::OTScopeBox)
	{
		return TargetMOI->GetObjectRotation().RotateVector(TargetMOI->GetNormal() * 0.5f * TargetMOI->GetExtents().Y + averageTargetPos);
	}
	else
	{
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

	const TArray<FVector> &targetCPs = TargetMOI->GetControlPoints();
	int32 numPolyPoints = targetCPs.Num();
	int32 edgeStartIdx = TargetIndex;
	int32 edgeEndIdx = (TargetIndex + 1) % numPolyPoints;

	const FVector &edgeStartPoint = TargetMOI->GetControlPoint(edgeStartIdx);
	const FVector &edgeEndPoint = TargetMOI->GetControlPoint(edgeEndIdx);
	FVector edgeDir = (edgeEndPoint - edgeStartPoint).GetSafeNormal();
	FVector edgeNormal = edgeDir ^ TargetMOI->GetNormal();

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
		OutWidgetSize = FVector2D(32.0f, 32.0f);
		OutMainButtonOffset = FVector2D(16.0f, 0.0f);
	}
	else
	{
		OutButtonStyle = PlayerHUD->HandleAssets->GenericPointStyle;
		OutWidgetSize = FVector2D(16.0f, 16.0f);
	}

	return true;
}


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

	OriginalControlPoints = TargetMOI->GetControlPoints();
	OriginalPlane = FPlane(TargetMOI->GetControlPoints().Num() > 0 ? TargetMOI->GetControlPoint(0) : TargetMOI->GetObjectLocation(), TargetMOI->GetNormal());
	OriginalExtrusion = TargetMOI->GetExtents().Y;
	LastValidExtrusion = OriginalExtrusion;
	AnchorLoc = GetHandlePosition();

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
	if (snapCursor || snapStructural)
	{
		float newExtrusion = OriginalExtrusion + extrusionDelta;
		if ((newExtrusion > 0.0f) && (newExtrusion != LastValidExtrusion))
		{
			FVector extents = TargetMOI->GetExtents();
			extents.Y = newExtrusion;
			TargetMOI->SetExtents(extents);

			LastValidExtrusion = newExtrusion;

			if (Sign < 0.0f)
			{
				for (int32 i = 0; i < TargetMOI->GetControlPoints().Num(); ++i)
				{
					TargetMOI->SetControlPoint(i,OriginalControlPoints[i] - (OriginalPlane * extrusionDelta));
				}
			}

			UpdateTargetGeometry();
		}
	}
	FVector camDir = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	camDir.Z = 0.f;
	camDir = camDir.RotateAngleAxis(90.f, FVector::UpVector);

	// Dim string for vertical extrusion handle - Delta
	UModumateFunctionLibrary::AddNewDimensionString(
		Controller,
		AnchorLoc,
		GetHandlePosition(),
		camDir,
		Controller->DimensionStringGroupID_Default,
		Controller->DimensionStringUniqueID_Delta,
		0,
		TargetMOI->GetActor(),
		EDimStringStyle::Fixed);

	FVector bottomStringLoc = FVector(GetHandlePosition().X, GetHandlePosition().Y, TargetMOI->GetControlPoint(0).Z);
	FVector topStringLoc = bottomStringLoc + FVector(0.f, 0.f, TargetMOI->GetExtents().Y);

	// Dim string for vertical extrusion handle - Total
	UModumateFunctionLibrary::AddNewDimensionString(
		Controller,
		bottomStringLoc,
		topStringLoc,
		camDir,
		Controller->DimensionStringGroupID_Default,
		Controller->DimensionStringUniqueID_Total,
		1,
		TargetMOI->GetActor(),
		EDimStringStyle::Fixed);
	return true;
}

FVector AAdjustPolyExtrusionHandle::GetHandlePosition() const
{
	if (TargetMOI->GetControlPoints().Num() > 0)
	{
		FVector objectNormal = TargetMOI->GetNormal();
		FVector offset = (Sign > 0.0f) ? (TargetMOI->GetExtents().Y * objectNormal) : FVector::ZeroVector;
		FVector centroid = Algo::Accumulate(
			TargetMOI->GetControlPoints(), 
			FVector::ZeroVector,
			[](FVector centroid, const FVector &p) { return centroid + p; }) / TargetMOI->GetControlPoints().Num();

		return offset + centroid;
	}
	else
	{
		return TargetMOI->GetObjectLocation();
	}
}

FVector AAdjustPolyExtrusionHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? Sign * TargetMOI->GetNormal() : FVector::ZeroVector;
}

bool AAdjustPolyExtrusionHandle::HandleInputNumber(float number)
{
	TArray<FVector> newPoints;
	FVector newExtents(ForceInitToZero);

	if (Controller->EMPlayerState->CurrentDimensionStringWithInputUniqueID == Controller->DimensionStringUniqueID_Delta)
	{
		float dir = FMath::Sign(TargetMOI->GetExtents().Y - OriginalExtrusion);
		if (Sign > 0.f)
		{
			newPoints = TargetMOI->GetControlPoints();
		}
		else
		{
			newPoints = OriginalControlPoints;
			for (int32 i = 0; i < newPoints.Num(); ++i)
			{
				newPoints[i].Z = newPoints[i].Z - number * dir;
			}
		}
		newExtents.Y = OriginalExtrusion + number * dir;
	}
	else
	{
		newExtents.Y = number;
		newPoints = TargetMOI->GetControlPoints();
	}

	TargetMOI->SetControlPoints(newPoints);
	TargetMOI->SetExtents(newExtents);
	Super::EndUse();
	return true;
}

bool AAdjustPolyExtrusionHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->GenericArrowStyle;
	OutWidgetSize = FVector2D(32.0f, 32.0f);
	OutMainButtonOffset = FVector2D(16.0f, 0.0f);
	return true;
}


const float AAdjustInvertHandle::DesiredWorldDist = 25.0f;
const float AAdjustInvertHandle::MaxScreenDist = 30.0f;

bool AAdjustInvertHandle::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	TargetMOI->SetObjectInverted(!TargetMOI->GetObjectInverted());

	EndUse();
	return false;
}

FVector AAdjustInvertHandle::GetHandlePosition() const
{
	if (!ensure(Controller && TargetMOI))
	{
		return FVector::ZeroVector;
	}

	FVector objectPos = TargetMOI->GetObjectLocation();
	FVector attachNormal = -FVector::UpVector;

	FVector attachPosWorld;
	FVector2D attachPosScreen;
	if (Controller->GetScreenScaledDelta(objectPos, attachNormal, DesiredWorldDist, MaxScreenDist, attachPosWorld, attachPosScreen))
	{
		return attachPosWorld;
	}

	return FVector::ZeroVector;
}

FVector AAdjustInvertHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? TargetMOI->GetNormal() : FVector::ZeroVector;
}

bool AAdjustInvertHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->InvertStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);

	return true;
}


const float AJustificationHandle::DesiredWorldDist = 30.0f;
const float AJustificationHandle::MaxScreenDist = 50.0f;

bool AJustificationHandle::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	if ((HandleChildren.Num() > 0) && !bRootExpanded)
	{
		for (AAdjustmentHandleActor *handleChild : HandleChildren)
		{
			if (handleChild)
			{
				handleChild->SetEnabled(true);
			}
		}

		bRootExpanded = true;
		ApplyWidgetStyle();
		AbortUse();
	}
	else
	{
		TargetMOI->SetExtents(FVector(JustificationValue, 0.0f, 0.0f));
		EndUse();
	}

	return false;
}

void AJustificationHandle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// TODO: replace with line actors, rather than immediate-mode HUD lines
	if (bEnabled)
	{
		FVector handleDirection = TargetMOI->GetNormal();
		FVector attachLocation = GetHandlePosition();

		// Draw line from attachLocation to center of ParentMOI
		Modumate::FModumateObjectInstance* parentMetaplane = TargetMOI->GetParentObject();
		if (parentMetaplane != nullptr && parentMetaplane->GetObjectType() == EObjectType::OTMetaPlane)
		{
			FVector metaPlaneMidpoint = parentMetaplane->GetObjectLocation();
			if (!metaPlaneMidpoint.Equals(attachLocation))
			{
				// Make new line
				FModumateLines newLine;
				newLine.Point1 = metaPlaneMidpoint;
				newLine.Point2 = attachLocation;
				newLine.Color = FLinearColor::White;
				newLine.Thickness = 2.f;
				Controller->HUDDrawWidget->LinesToDraw.Add(newLine);
			}
		}
	}
}

FVector AJustificationHandle::GetHandlePosition() const
{
	if (!ensure(Controller && TargetMOI))
	{
		return FVector::ZeroVector;
	}

	FVector objectPos = TargetMOI->GetObjectLocation();
	FVector objectNormalWorld = TargetMOI->GetNormal();

	FVector worldP0, worldP1;
	FVector2D screenP0, screenP1;
	if (Controller->GetScreenScaledDelta(objectPos, objectNormalWorld, -DesiredWorldDist, MaxScreenDist, worldP0, screenP0) &&
		Controller->GetScreenScaledDelta(objectPos, objectNormalWorld, DesiredWorldDist, MaxScreenDist, worldP1, screenP1))
	{
		return FMath::Lerp(worldP0, worldP1, JustificationValue);
	}

	return FVector::ZeroVector;
}

FVector AJustificationHandle::GetHandleDirection() const
{
	return ensure(TargetMOI) ? TargetMOI->GetNormal() : FVector::ZeroVector;
}

void AJustificationHandle::SetJustification(float InJustificationValue)
{
	JustificationValue = InJustificationValue;
}

void AJustificationHandle::SetEnabled(bool bNewEnabled)
{
	Super::SetEnabled(bNewEnabled);

	if (bNewEnabled)
	{
		ApplyWidgetStyle();
	}
	else
	{
		bRootExpanded = false;
	}
}

bool AJustificationHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	if ((HandleChildren.Num() > 0) && !bRootExpanded)
	{
		OutButtonStyle = PlayerHUD->HandleAssets->JustificationRootStyle;
		OutWidgetSize = FVector2D(64.0f, 64.0f);
	}
	else
	{
		OutButtonStyle = PlayerHUD->HandleAssets->GenericPointStyle;
		OutWidgetSize = FVector2D(16.0f, 16.0f);
	}

	return true;
}
