#include "ToolsAndAdjustments/Handles/AdjustPolyExtrusionHandle.h"

#include "Algo/Accumulate.h"
#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

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
	OutWidgetSize = FVector2D(16.0f, 16.0f);
	OutMainButtonOffset = FVector2D(16.0f, 0.0f);
	return true;
}
