// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/AdjustmentHandleWidget.h"

using namespace Modumate;
bool AAdjustPortalInvertHandle::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	TArray<int32> ids = { TargetMOI->ID };

	if (bShouldTransvert)
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kTransverseObjects)
			.Param(Parameters::kObjectIDs, ids));
	}
	else
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kInvertObjects)
			.Param(Parameters::kObjectIDs, ids));
	}

	EndUse();
	return false;
}

FVector AAdjustPortalInvertHandle::GetHandlePosition() const
{
	FVector bottomCorner0, bottomCorner1;
	if (!GetBottomCorners(bottomCorner0, bottomCorner1))
	{
		return FVector::ZeroVector;
	}

	if (bShouldTransvert)
	{
		return FMath::Lerp(bottomCorner0, bottomCorner1, 0.25f);
	}
	else
	{
		return FMath::Lerp(bottomCorner0, bottomCorner1, 0.75f);
	}
}

FVector AAdjustPortalInvertHandle::GetHandleDirection() const
{
	if (bShouldTransvert)
	{
		return ensure(TargetMOI) ? TargetMOI->GetNormal() : FVector::ZeroVector;
	}
	else
	{
		return ensure(TargetMOI) ? (FVector::UpVector ^ TargetMOI->GetNormal()).GetSafeNormal() : FVector::ZeroVector;
	}
}

void AAdjustPortalInvertHandle::SetTransvert(bool bInShouldTransvert)
{
	bShouldTransvert = bInShouldTransvert;
}

bool AAdjustPortalInvertHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->InvertStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);
	return true;
}

bool AAdjustPortalInvertHandle::GetBottomCorners(FVector &OutCorner0, FVector &OutCorner1) const
{
	if (!ensure(TargetMOI))
	{
		return false;
	}

	FVector bottomFrontCorner0 = TargetMOI->GetCorner(0);
	FVector bottomFrontCorner1 = TargetMOI->GetCorner(3);
	FVector bottomBackCorner0 = TargetMOI->GetCorner(4);
	FVector bottomBackCorner1 = TargetMOI->GetCorner(7);

	OutCorner0 = 0.5f * (bottomFrontCorner0 + bottomBackCorner0);
	OutCorner1 = 0.5f * (bottomFrontCorner1 + bottomBackCorner1);
	return true;
}


bool AAdjustPortalPointHandle::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	OriginalLocHandle = GetHandlePosition();
	OrginalLoc = TargetMOI->GetActor()->GetActorLocation();
	OriginalP = TargetMOI->GetControlPoints();
	FVector actorLoc = GetHandlePosition();
	AnchorLoc = FVector(actorLoc.X, actorLoc.Y, TargetMOI->GetActor()->GetActorLocation().Z);
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector::UpVector);

	TargetMOI->ShowAdjustmentHandles(Controller, false);

	return true;
}

bool AAdjustPortalPointHandle::UpdateUse()
{
	Super::UpdateUse();

	auto *wallParent = TargetMOI->GetParentObject();
	bool bValidParentWall = wallParent && wallParent->GetObjectType() == EObjectType::OTWallSegment;

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return true;
	}

	// TODO: remove ASAP
	float mousex, mousey;
	if (!Controller->GetMousePosition(mousex, mousey))
	{
		return false;
	}

	FVector hitPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	FVector worldCP0 = UKismetMathLibrary::TransformLocation(TargetMOI->GetActor()->GetActorTransform(), TargetMOI->GetControlPoint(0));
	FVector worldCP3 = UKismetMathLibrary::TransformLocation(TargetMOI->GetActor()->GetActorTransform(), TargetMOI->GetControlPoint(3));

	// Limit hit to plane form by MOI toward the camera
	FVector dir, projectLoc;
	Controller->DeprojectScreenPositionToWorld(mousex, mousey, projectLoc, dir);
	FVector hitPointDir = UKismetMathLibrary::GetDirectionUnitVector(projectLoc, hitPoint);
	FVector projectLocEnd = projectLoc + (hitPointDir * 1000000.f);
	FVector planeNormal = UKismetMathLibrary::RotateAngleAxis(UKismetMathLibrary::GetDirectionUnitVector(worldCP0, worldCP3), -90.0, FVector(0.0, 0.0, 1.0));
	float interectionT;
	FVector hitIntersection;
	bool bHitIntersect = UKismetMathLibrary::LinePlaneIntersection_OriginNormal(projectLoc, projectLocEnd, OriginalLocHandle, planeNormal, interectionT, hitIntersection);
	if (!bHitIntersect)
	{
		return true;
	}
	bool cpEdgeCheck = UModumateFunctionLibrary::IsTargetCloseToControlPointEdges(hitIntersection, TargetMOI->GetControlPoints(), TargetMOI->GetActor()->GetActorTransform(), 10.f);
	if (Controller->EMPlayerState->SnappedCursor.Visible && !cpEdgeCheck)
	{
		switch (Controller->EMPlayerState->SnappedCursor.SnapType)
		{
		case ESnapType::CT_CORNERSNAP:
		case ESnapType::CT_MIDSNAP:
		case ESnapType::CT_EDGESNAP:
		case ESnapType::CT_FACESELECT:
		{
			FVector lineDirection = TargetMOI->GetObjectRotation().Vector();
			hitIntersection = UKismetMathLibrary::FindClosestPointOnLine(hitPoint, hitIntersection, lineDirection);

			// Draw affordance lines if there is snapping
			FAffordanceLine affordanceH;
			affordanceH.Color = FLinearColor(FColorList::Black);
			affordanceH.EndPoint = FVector(hitPoint.X, hitPoint.Y, GetHandlePosition().Z);
			affordanceH.StartPoint = GetHandlePosition();
			affordanceH.Interval = 4.0f;
			Controller->EMPlayerState->AffordanceLines.Add(affordanceH);
			FAffordanceLine affordanceV;
			affordanceV.Color = FLinearColor::Blue;
			affordanceV.EndPoint = hitPoint;
			affordanceV.StartPoint = FVector(hitPoint.X, hitPoint.Y, GetHandlePosition().Z);
			affordanceV.Interval = 4.0f;
			Controller->EMPlayerState->AffordanceLines.Add(affordanceV);
			break;
		}
		default: break;
		}
	}

	if (TargetMOI->GetObjectType() == EObjectType::OTDoor) // Doors cannot move vertically, limit movement to control point 0 and 1
	{
		hitIntersection.Z = worldCP0.Z;
		FVector moveDiff;
		if (TargetIndex == 0 || TargetIndex == 1 || TargetIndex == 4 || TargetIndex == 5)
		{
			moveDiff = TargetMOI->GetControlPoint(0) - UKismetMathLibrary::InverseTransformLocation(TargetMOI->GetActor()->GetActorTransform(), hitIntersection);
		}
		else
		{
			moveDiff = TargetMOI->GetControlPoint(3) - UKismetMathLibrary::InverseTransformLocation(TargetMOI->GetActor()->GetActorTransform(), hitIntersection);
		}

		for (int32 i = 0; i < 4; ++i)
		{
			FVector cp = TargetMOI->GetControlPoint(i);
			cp.X -= moveDiff.X;
			TargetMOI->SetControlPoint(i, cp);
		}
	}
	else
	{
		FVector moveDiff;
		int32 handleCPConvert = TargetIndex % 4;
		moveDiff = TargetMOI->GetControlPoint(handleCPConvert) - UKismetMathLibrary::InverseTransformLocation(TargetMOI->GetActor()->GetActorTransform(), hitIntersection);

		for (int32 i = 0; i < 4; ++i)
		{
			FVector cp = TargetMOI->GetControlPoint(i);
			cp.X -= moveDiff.X;
			TargetMOI->SetControlPoint(i, cp);
		}
		//Replace with below if want to freely move vertically
		//MOI->ControlPoints[0] -= moveDiff;
		//MOI->ControlPoints[1] -= moveDiff;
		//MOI->ControlPoints[2] -= moveDiff;
		//MOI->ControlPoints[3] -= moveDiff;
	}
	// Point horizontal dim string. Delta only
	FVector offsetDir = TargetMOI->GetActor()->GetActorRightVector();
	UModumateFunctionLibrary::AddNewDimensionString(
		Controller,
		OriginalLocHandle,
		GetHandlePosition(),
		offsetDir,
		Controller->DimensionStringGroupID_Default,
		Controller->DimensionStringUniqueID_Delta,
		0,
		TargetMOI->GetActor(),
		EDimStringStyle::HCamera);

	TargetMOI->UpdateGeometry();
	if (bValidParentWall)
	{
		wallParent->UpdateGeometry();
	}

	return true;
}

void AAdjustPortalPointHandle::EndUse()
{
	FVector newLocation = TargetMOI->GetObjectLocation();
	TArray<FVector> newCPs = TargetMOI->GetControlPoints();

	AbortUse();

	Controller->ModumateCommand(
		FModumateCommand(Commands::kUpdateMOIHoleParams)
		.Param(Parameters::kObjectID, TargetMOI->ID)
		.Param(Parameters::kLocation, newLocation)
		.Param(Parameters::kControlPoints, newCPs));
}

void AAdjustPortalPointHandle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector thisPosition = TargetMOI->GetCorner(TargetIndex);
	FVector oppositePosition = TargetMOI->GetCorner((TargetIndex + 4) % 8);

	APlayerCameraManager* cameraActor = UGameplayStatics::GetPlayerCameraManager(this, 0);
	FVector cameraLoc = cameraActor->GetCameraLocation();
	bool bShouldBeVisible = FVector::Dist(thisPosition, cameraLoc) < FVector::Dist(oppositePosition, cameraLoc);

	Widget->SetVisibility(bShouldBeVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

FVector AAdjustPortalPointHandle::GetHandlePosition() const
{
	return TargetMOI->GetCorner(TargetIndex);
}

bool AAdjustPortalPointHandle::HandleInputNumber(float number)
{
	FVector currentDirection = (TargetMOI->GetControlPoint(0) - OriginalP[0]).GetSafeNormal();

	TArray<FVector> newCPs;
	for (int32 i = 0; i < OriginalP.Num(); ++i)
	{
		newCPs.Add(OriginalP[i] + (currentDirection * number));
	}
	TargetMOI->SetControlPoints(OriginalP);

	Controller->ModumateCommand(
		FModumateCommand(Commands::kUpdateMOIHoleParams)
		.Param(Parameters::kObjectID, TargetMOI->ID)
		.Param(Parameters::kLocation, TargetMOI->GetObjectLocation())
		.Param(Parameters::kControlPoints, newCPs));

	EndUse();
	return true;
}

bool AAdjustPortalPointHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->GenericPointStyle;
	OutWidgetSize = FVector2D(12.0f, 12.0f);
	return true;
}
