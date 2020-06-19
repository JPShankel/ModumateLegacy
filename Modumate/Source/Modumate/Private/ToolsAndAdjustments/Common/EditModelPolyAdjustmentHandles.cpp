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

	OriginalP.Reset();
	for (int32 cp : CP)
	{
		OriginalP.Add(TargetMOI->GetControlPoint(cp));
		LastValidPendingCPLocations.Add(TargetMOI->GetControlPoint(cp));
	}

	FVector targetNormal = TargetMOI->GetNormal();
	if (!targetNormal.IsNormalized() || (OriginalP.Num() == 0))
	{
		return false;
	}

	PolyPlane = FPlane(OriginalP[0], targetNormal);

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

	if (OriginalP.Num() == CP.Num())
	{
		if (CP.Num() == 2)
		{
			int32 numTotalCPs = TargetMOI->GetControlPoints().Num();
			FVector closestPoint1, closestPoint2;
			FVector currentEdgeDirection = (TargetMOI->GetControlPoint(CP[1]) - TargetMOI->GetControlPoint(CP[0])).GetSafeNormal();

			// Intersection test for first CP
			int32 startID0 = (CP[0] + numTotalCPs - 1) % numTotalCPs;
			int32 endID0 = CP[0];
			FVector lineDirection0 = (TargetMOI->GetControlPoint(endID0) - TargetMOI->GetControlPoint(startID0)).GetSafeNormal();
			bool b1 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, hitPoint, currentEdgeDirection, TargetMOI->GetControlPoint(endID0), lineDirection0);
			FVector newCP0 = closestPoint1;

			// Intersection test for second CP
			int32 startID1 = (CP[1] + 1) % numTotalCPs;
			int32 endID1 = CP[1];
			FVector lineDirection1 = (TargetMOI->GetControlPoint(endID1) - TargetMOI->GetControlPoint(startID1)).GetSafeNormal();
			bool b2 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, hitPoint, currentEdgeDirection, TargetMOI->GetControlPoint(endID1), lineDirection1);
			FVector newCP1 = closestPoint1;

			// Set MOI control points to new CP
			TargetMOI->SetControlPoint(CP[0],newCP0);
			TargetMOI->SetControlPoint(CP[1],newCP1);

			// Check if CPs are intersect. If it is, don't update geometry
			// Store last good locations to fall back on if the new floor lacks proper triangulation
			if (UModumateGeometryStatics::ArePolygonEdgesValid(TargetMOI->GetControlPoints()))
			{
				UpdateTargetGeometry();
				LastValidPendingCPLocations = { TargetMOI->GetControlPoint(CP[0]) , TargetMOI->GetControlPoint(CP[1]) };
			}
			else
			{
				TargetMOI->SetControlPoint(CP[0],LastValidPendingCPLocations[0]);
				TargetMOI->SetControlPoint(CP[1],LastValidPendingCPLocations[1]);
			}

			HandleOriginalPoint = (OriginalP[0] + OriginalP[1]) / 2.f;
			FVector curSideMidLoc = (TargetMOI->GetControlPoint(CP[0]) + TargetMOI->GetControlPoint(CP[1])) / 2.f;
			currentEdgeDirection = (TargetMOI->GetControlPoint(CP[1]) - TargetMOI->GetControlPoint(CP[0])).GetSafeNormal();
			FVector edgeNormal = GetHandleDirection();
			bool b3 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, HandleOriginalPoint, edgeNormal, curSideMidLoc, currentEdgeDirection);
			HandleCurrentPoint = closestPoint1;

			// Dim string. between cursor pos and original handle pos. Delta only
			UModumateFunctionLibrary::AddNewDimensionString(Controller,
				HandleOriginalPoint,
				HandleCurrentPoint,
				currentEdgeDirection,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0,
				TargetMOI->GetActor(),
				EDimStringStyle::HCamera,
				EEnterableField::NonEditableText,
				20.f,
				EAutoEditableBox::UponUserInput_SameGroupIndex,
				true);
		}
		else if (CP.Num() == 1)
		{
			TargetMOI->SetControlPoint(CP[0],OriginalP[0] + dp);
			if (UModumateGeometryStatics::ArePolygonEdgesValid(TargetMOI->GetControlPoints()))
			{
				UpdateTargetGeometry();
				LastValidPendingCPLocations = { TargetMOI->GetControlPoint(CP[0]) };
			}
			else
			{
				TargetMOI->SetControlPoint(CP[0],LastValidPendingCPLocations[0]);
			}

			FVector offsetDir = dp.GetSafeNormal() ^ FVector(Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

			// Dim string between cursor pos and original point handle position. Delta only
			UModumateFunctionLibrary::AddNewDimensionString(Controller,
				OriginalP[0],
				TargetMOI->GetControlPoint(CP[0]),
				offsetDir,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0,
				TargetMOI->GetActor(),
				EDimStringStyle::HCamera,
				EEnterableField::NonEditableText,
				40.f,
				EAutoEditableBox::Never);
		}
	}
	return true;
}

FVector AAdjustPolyPointHandle::GetHandlePosition() const
{
	if (!ensure(TargetMOI))
	{
		return FVector::ZeroVector;
	}

	int32 numTargetCP = CP.Num();
	FVector averageTargetPos(ForceInitToZero);
	for (int32 cpIndex : CP)
	{
		if (!ensure(TargetMOI->GetControlPoints().IsValidIndex(cpIndex)))
		{
			return FVector::ZeroVector;
		}

		averageTargetPos += TargetMOI->GetControlPoint(cpIndex);
	}
	averageTargetPos /= numTargetCP;

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

	const TArray<FVector> &targetCPs = TargetMOI->GetControlPoints();
	int32 numCPs = targetCPs.Num();
	if (!ensure(numCPs >= 2) || (CP.Num() != 2) || (CP[1] != ((CP[0] + 1) % targetCPs.Num())))
	{
		return FVector::ZeroVector;
	}

	FVector sidePoint0 = TargetMOI->GetControlPoint(CP[0]);
	FVector sidePoint1 = TargetMOI->GetControlPoint(CP[1]);
	FVector sideDir = (sidePoint1 - sidePoint0).GetSafeNormal();
	FVector sideNormal = sideDir ^ TargetMOI->GetNormal();

	return sideNormal;
}

bool AAdjustPolyPointHandle::HandleInputNumber(float number)
{
	// Handle should only affect two control points
	if (CP.Num() == 2)
	{
		TArray<FVector> proxyCPs = TargetMOI->GetControlPoints();
		FVector newCP0;
		FVector newCP1;
			
		FName curDimStringID = Controller->EMPlayerState->CurrentDimensionStringWithInputUniqueID;
		// Identify if the current dim string is parallel to both edges control by this handle, or is it only changing delta length
		if (curDimStringID == Controller->DimensionStringUniqueID_Delta || curDimStringID == Controller->DimensionStringUniqueID_Total)
		{
			FVector pendingHandleDirection = (HandleCurrentPoint - HandleOriginalPoint).GetSafeNormal();
			// Calculate where the handle will be if it travels the input distance
			FVector curHandleLocation = (OriginalP[0] + OriginalP[1]) / 2.f;
			FVector inputHandleLocation = curHandleLocation;

			// Calculate length from handle location to opposite edge of the polygon
			// This methods treats both edges control by this handle are parallel
			if (curDimStringID == Controller->DimensionStringUniqueID_Total)
			{
				FVector sideCP0 = OriginalP[0];
				FVector sideCP1 = OriginalP[1];
				FVector preCP0 = TargetMOI->GetControlPoint((CP[0] + proxyCPs.Num() - 1) % proxyCPs.Num());
				FVector postCP1 = TargetMOI->GetControlPoint((CP[1] + 1) % proxyCPs.Num());
				FVector originalDir = (sideCP0 - preCP0).GetSafeNormal();
				inputHandleLocation = ((preCP0 + postCP1) * 0.5f) + (originalDir * number);
			}
			// else the input is added to original handle location (delta)
			else
			{
				inputHandleLocation = curHandleLocation + (pendingHandleDirection * number);
			}

			FVector currentEdgeDirection = UKismetMathLibrary::GetDirectionUnitVector(OriginalP[0], OriginalP[1]);
			FVector closestPoint1, closestPoint2;

			// Intersection test for first CP
			int32 startID0 = (CP[0] + proxyCPs.Num() - 1) % proxyCPs.Num();
			int32 endID0 = CP[0];
			FVector lineDirection0 = UKismetMathLibrary::GetDirectionUnitVector(TargetMOI->GetControlPoint(startID0), TargetMOI->GetControlPoint(endID0));
			bool b1 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, inputHandleLocation, currentEdgeDirection, OriginalP[0], lineDirection0);
			newCP0 = FVector(closestPoint1.X, closestPoint1.Y, closestPoint1.Z);

			// Intersection test for second CP
			int32 startID1 = (CP[1] + 1) % proxyCPs.Num();
			int32 endID1 = CP[1];
			FVector lineDirection1 = UKismetMathLibrary::GetDirectionUnitVector(TargetMOI->GetControlPoint(startID1), TargetMOI->GetControlPoint(endID1));
			bool b2 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, inputHandleLocation, currentEdgeDirection, OriginalP[1], lineDirection1);
			newCP1 = FVector(closestPoint1.X, closestPoint1.Y, closestPoint1.Z);

			proxyCPs[CP[0]] = newCP0;
			proxyCPs[CP[1]] = newCP1;
		}
		// Edges aren't parallel. Set the length of one of the side edges while preserving the direction of the handle
		else if (curDimStringID == Controller->DimensionStringUniqueID_SideEdge0 || curDimStringID == Controller->DimensionStringUniqueID_SideEdge1)
		{
			FVector sideCP0 = OriginalP[0];
			FVector sideCP1 = OriginalP[1];
			FVector preCP0 = TargetMOI->GetControlPoint((CP[0] + proxyCPs.Num() - 1) % proxyCPs.Num());
			FVector postCP1 = TargetMOI->GetControlPoint((CP[1] + 1) % proxyCPs.Num());
			FVector edgeDir = (sideCP0 - sideCP1).GetSafeNormal();
			FVector intersectPoint = FVector::ZeroVector;
				
			if (curDimStringID == Controller->DimensionStringUniqueID_SideEdge0)
			{
				FVector dir = (sideCP0 - preCP0).GetSafeNormal();
				newCP0 = preCP0 + (dir * number);
				UModumateFunctionLibrary::ClosestPointsOnTwoLines(newCP1, intersectPoint, newCP0, edgeDir, sideCP1, (sideCP1 - postCP1).GetSafeNormal());
			}
			else
			{
				FVector dir = (sideCP1 - postCP1).GetSafeNormal();
				newCP1 = postCP1 + (dir * number);
				UModumateFunctionLibrary::ClosestPointsOnTwoLines(newCP0, intersectPoint, newCP1, edgeDir, sideCP0, (sideCP0 - preCP0).GetSafeNormal());
			}

			proxyCPs[CP[0]] = newCP0;
			proxyCPs[CP[1]] = newCP1;
		}
		// Cannot determine method of calculating new length based on dim string UniqueID
		else
		{
			return false;
		}

		// Check if CPs are intersect. If it is, don't update geometry
		if (UModumateGeometryStatics::ArePolygonEdgesValid(proxyCPs))
		{
			// Set MOI control points to new CP
			TargetMOI->SetControlPoint(CP[0],newCP0);
			TargetMOI->SetControlPoint(CP[1],newCP1);
			EndUse();
			return true;
		}
		else
		{
			// The result triangulation will create invalid geometry
			return false;
		}
	}
	return false; // false by default if not handling numbers
}

void AAdjustPolyPointHandle::SetAdjustPolyEdge(bool bInAdjustPolyEdge)
{
	bAdjustPolyEdge = bInAdjustPolyEdge;
}

void AAdjustPolyPointHandle::Initialize()
{
	Super::Initialize();

	if (TargetMOI)
	{
		const TArray<FVector> &targetCPs = TargetMOI->GetControlPoints();
		int32 numCPs = targetCPs.Num();

		// TODO: remove this when 'CP' is no longer used, as it's the deprecated way to keep track of handle target side/index
		CP.Reset();
		if (bAdjustPolyEdge)
		{
			CP.Add(TargetIndex);
			CP.Add((TargetIndex + 1) % numCPs);
		}
		else
		{
			CP.Add(TargetIndex);
		}
	}
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
