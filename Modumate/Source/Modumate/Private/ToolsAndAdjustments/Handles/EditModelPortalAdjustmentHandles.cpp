// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelPortalAdjustmentHandles.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelGameState_CPP.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateCommands.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Core/Public/Misc/OutputDeviceNull.h"
#include "Engine.h"
#include "DrawDebugHelpers.h"

namespace Modumate
{
	bool FAdjustPortalSideHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustHeightDoorHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		// TODO: remove ASAP
		float mousex, mousey;
		if (!Controller->GetMousePosition(mousex, mousey))
		{
			return false;
		}

		MOI->ShowAdjustmentHandles(Controller.Get(), false);
		FVector dir, projectLoc;
		Controller->DeprojectScreenPositionToWorld(mousex, mousey, projectLoc, dir);

		BeginUpdateHolePonts = MOI->ControlPoints;
		HoleActorBeginLocation = MOI->GetActor()->GetActorLocation();

		if (Side >= 0 && Side <= 3)
		{
			TArray<FVector> worldCPs;
			for (int32 i = 0; i < MOI->ControlPoints.Num(); ++i)
			{
				worldCPs.Add(UKismetMathLibrary::TransformLocation(MOI->GetActor()->GetActorTransform(), MOI->ControlPoints[i]));
			}
			if (Side == 0)
			{
				PlanePoint = worldCPs[0];
			}
			else if (Side == 1)
			{
				PlanePoint = worldCPs[3];
			}
			else if (Side == 2)
			{
				PlanePoint = worldCPs[1];
			}
			else if (Side == 3)
			{
				PlanePoint = worldCPs[0];
			}
			PlaneNormal = UKismetMathLibrary::RotateAngleAxis(UKismetMathLibrary::GetDirectionUnitVector(worldCPs[0], worldCPs[3]), -90.0, FVector(0.0, 0.0, 1.0));
			AnchorPoint = PlanePoint;
			Controller->DeprojectScreenPositionToWorld(mousex, mousey, projectLoc, dir);
			FVector hitPoint = FMath::LinePlaneIntersection(projectLoc, projectLoc + dir, PlanePoint, PlaneNormal);
			FVector dH = hitPoint - AnchorPoint;
			FVector transformedDH = (MOI->GetActor()->GetActorRotation().UnrotateVector(dH));

			if (Side == 0)
			{
				MOI->ControlPoints[0] = BeginUpdateHolePonts[0] + FVector(0.0, transformedDH.Y, 0.0);
				MOI->ControlPoints[1] = BeginUpdateHolePonts[1] + FVector(0.0, transformedDH.Y, 0.0);
				MOI->UpdateGeometry();
			}
			else if (Side == 1)
			{
				MOI->ControlPoints[2] = BeginUpdateHolePonts[2];
				MOI->ControlPoints[3] = BeginUpdateHolePonts[3];
				MOI->UpdateGeometry();
			}
			else if (Side == 2)
			{
				MOI->ControlPoints[1] = BeginUpdateHolePonts[1] + FVector(0.0, 0.0, dH.Z);
				MOI->ControlPoints[2] = BeginUpdateHolePonts[2] + FVector(0.0, 0.0, dH.Z);
				MOI->UpdateGeometry();
			}
			else if (Side == 3)
			{
				MOI->ControlPoints[0] = BeginUpdateHolePonts[0];
				MOI->ControlPoints[3] = BeginUpdateHolePonts[3];
				MOI->UpdateGeometry();
			}
		}
		return true;
	};

	bool FAdjustPortalSideHandle::OnUpdateUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustHeightDoorHandle::OnUpdateUse"));
		FEditModelAdjustmentHandleBase::OnUpdateUse();

		// TODO: remove ASAP
		float mousex, mousey;
		if (!Controller->GetMousePosition(mousex, mousey))
		{
			return false;
		}

		FVector dir, projectLoc;
		Controller->DeprojectScreenPositionToWorld(mousex, mousey, projectLoc, dir);
		FVector hitPoint = FMath::LinePlaneIntersection(projectLoc, projectLoc + dir, PlanePoint, PlaneNormal);
		FVector snapHitPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;

		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return true;
		}

		Modumate::FModumateDocument *doc = &Cast<AEditModelGameState_CPP>(Handle->GetWorld()->GetGameState())->Document;

		bool structuralHit = false;

		switch (Controller->EMPlayerState->SnappedCursor.SnapType)
		{
		case ESnapType::CT_CORNERSNAP:
		case ESnapType::CT_MIDSNAP:
		case ESnapType::CT_EDGESNAP:
		case ESnapType::CT_FACESELECT:
		{
			structuralHit = true;
			bool drawAffordanceLine = false;
			bool cpEdgeCheck = UModumateFunctionLibrary::IsTargetCloseToControlPointEdges(hitPoint, MOI->ControlPoints, MOI->GetActor()->GetActorTransform(), 10.f);
			if (Side == 2 || Side == 3) // Change Z axis only for vertical stretch handles
			{
				if ((!cpEdgeCheck) || (Controller->EMPlayerState->SnappedCursor.SnapType != ESnapType::CT_FACESELECT))
				{
					hitPoint.Z = snapHitPoint.Z;
					drawAffordanceLine = true;
				}
			}
			else if((!cpEdgeCheck) || (Controller->EMPlayerState->SnappedCursor.SnapType != ESnapType::CT_FACESELECT))
			{
				hitPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
				drawAffordanceLine = true;
			}
			// Draw affordance lines if there is snapping
			if (drawAffordanceLine)
			{
				FAffordanceLine affordanceH;
				affordanceH.Color = FLinearColor(FColorList::Black);
				affordanceH.EndPoint = FVector(snapHitPoint.X, snapHitPoint.Y, GetAttachmentPoint().Z);
				affordanceH.StartPoint = GetAttachmentPoint();
				affordanceH.Interval = 4.0f;
				Controller->EMPlayerState->AffordanceLines.Add(affordanceH);
				FAffordanceLine affordanceV;
				affordanceV.Color = FLinearColor::Blue;
				affordanceV.EndPoint = snapHitPoint;
				affordanceV.StartPoint = FVector(snapHitPoint.X, snapHitPoint.Y, GetAttachmentPoint().Z);
				affordanceV.Interval = 4.0f;
				Controller->EMPlayerState->AffordanceLines.Add(affordanceV);
			}
			break;
		}
		default: break;
		}

		FVector cpPortal0 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[0]);
		FVector cpPortal1 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[1]);
		FVector cpPortal2 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[2]);
		FVector cpPortal3 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[3]);
		float cpPortalHalfHeight = (cpPortal1 + cpPortal0).Z / 2.f;

		if (Side == 0) // Horizontal Side Left
		{
			FVector dH = hitPoint - AnchorPoint;
			FVector transformedDH = (MOI->GetActor()->GetActorRotation().UnrotateVector(dH));
			if (dH.Size() > 1.0f)
			{
				MOI->ControlPoints[0] = BeginUpdateHolePonts[0] + FVector(transformedDH.X, 0.0f, 0.0f);
				MOI->ControlPoints[1] = BeginUpdateHolePonts[1] + FVector(transformedDH.X, 0.0f, 0.0f);
			}
			// Dim String - total
			FVector offsetDirTotal = MOI->GetActor()->GetActorUpVector();
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpPortal1,
				cpPortal2,
				offsetDirTotal,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				1,
				MOI->GetActor(),
				EDimStringStyle::Fixed);

			FVector cpDeltaLocalPortalStart = MOI->GetActor()->GetActorTransform().TransformPosition(BeginUpdateHolePonts[1]);
			FVector cpDeltaLocalPortalEnd = cpPortal1;
			cpDeltaLocalPortalStart.Z = cpPortalHalfHeight;
			cpDeltaLocalPortalEnd.Z = cpPortalHalfHeight;
			FVector offsetDirDelta = MOI->GetActor()->GetActorRightVector();
			// Dim String - delta
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpDeltaLocalPortalStart,
				cpDeltaLocalPortalEnd,
				offsetDirDelta,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0,
				MOI->GetActor(),
				EDimStringStyle::HCamera);
		}

		if (Side == 1) // Horizontal Side Right
		{
			FVector dH = hitPoint - AnchorPoint;
			FVector transformedDH = (MOI->GetActor()->GetActorRotation().UnrotateVector(dH));
			if (dH.Size() > 1.0f)
			{
				MOI->ControlPoints[2] = BeginUpdateHolePonts[2] + FVector(transformedDH.X, 0.0f, 0.0f);
				MOI->ControlPoints[3] = BeginUpdateHolePonts[3] + FVector(transformedDH.X, 0.0f, 0.0f);
			}
			// Dim String - total
			FVector offsetDirTotal = MOI->GetActor()->GetActorUpVector();
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpPortal1,
				cpPortal2,
				offsetDirTotal,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				1, // Total Dim String
				MOI->GetActor(),
				EDimStringStyle::Fixed);

			FVector cpDeltaLocalPortalStart = MOI->GetActor()->GetActorTransform().TransformPosition(BeginUpdateHolePonts[2]);
			FVector cpDeltaLocalPortalEnd = cpPortal2;
			cpDeltaLocalPortalStart.Z = cpPortalHalfHeight;
			cpDeltaLocalPortalEnd.Z = cpPortalHalfHeight;
			FVector offsetDirDelta = MOI->GetActor()->GetActorRightVector();
			// Dim Sting - delta
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpDeltaLocalPortalStart,
				cpDeltaLocalPortalEnd,
				offsetDirDelta,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0, // Delta dim string
				MOI->GetActor(),
				EDimStringStyle::HCamera);
		}

		if (Side == 2) // Up
		{
			FVector dV = hitPoint - AnchorPoint;
			if (dV.Size() > 1.0f)
			{
				MOI->ControlPoints[1] = BeginUpdateHolePonts[1] + FVector(0.0, 0.0, dV.Z);
				MOI->ControlPoints[2] = BeginUpdateHolePonts[2] + FVector(0.0, 0.0, dV.Z);
			}
			FVector offsetDir = MOI->GetActor()->GetActorForwardVector() * FVector(-1.f);
			// Dim string - total
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpPortal0,
				cpPortal1,
				offsetDir,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				1,
				MOI->GetActor(),
				EDimStringStyle::Fixed);
			FVector cpDeltaLocalPortalStart1 = MOI->GetActor()->GetActorTransform().TransformPosition(BeginUpdateHolePonts[1]);
			FVector cpDeltaLocalPortalStart2 = MOI->GetActor()->GetActorTransform().TransformPosition(BeginUpdateHolePonts[2]);
			FVector cpDeltaLocalPortalStart = (cpDeltaLocalPortalStart1 + cpDeltaLocalPortalStart2) / 2.f;
			FVector cpDeltaLocalPortalEnd = (cpPortal1 + cpPortal2) / 2.f;
			// Dim string - delta
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpDeltaLocalPortalStart,
				cpDeltaLocalPortalEnd,
				offsetDir,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0,
				MOI->GetActor(),
				EDimStringStyle::Fixed);
		}

		if (Side == 3) // Down
		{
			FVector dV = hitPoint - AnchorPoint;
			// Min. height limit
			float heightDiff = MOI->ControlPoints[1].Z - MOI->ControlPoints[0].Z;
			float moveIncrement = hitPoint.Z - MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[0]).Z;
			// Limit handle to move down only if portal size is too small.
			if ((dV.Size() > 1.0f && heightDiff > 1.0f) || moveIncrement < 0)
			{
				MOI->ControlPoints[0] = BeginUpdateHolePonts[0] + FVector(0.0, 0.0, dV.Z);
				MOI->ControlPoints[3] = BeginUpdateHolePonts[3] + FVector(0.0, 0.0, dV.Z);
			}
			FVector offsetDir = MOI->GetActor()->GetActorForwardVector() * FVector(-1.f);
			// Dim string - total
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpPortal0,
				cpPortal1,
				offsetDir,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				1,
				MOI->GetActor(),
				EDimStringStyle::Fixed);
			FVector cpDeltaLocalPortalStart0 = MOI->GetActor()->GetActorTransform().TransformPosition(BeginUpdateHolePonts[0]);
			FVector cpDeltaLocalPortalStart3 = MOI->GetActor()->GetActorTransform().TransformPosition(BeginUpdateHolePonts[3]);
			FVector cpDeltaLocalPortalStart = (cpDeltaLocalPortalStart0 + cpDeltaLocalPortalStart3) / 2.f;
			FVector cpDeltaLocalPortalEnd = (cpPortal0 + cpPortal3) / 2.f;
			// Dim string - delta
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				cpDeltaLocalPortalStart,
				cpDeltaLocalPortalEnd,
				offsetDir,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				0,
				MOI->GetActor(),
				EDimStringStyle::Fixed);
		}

		MOI->UpdateGeometry();

		auto *wallParent = MOI->GetParentObject();
		if (wallParent && wallParent->ObjectType == EObjectType::OTWallSegment)
		{
			wallParent->UpdateGeometry();
		}

		return true;
	};

	bool FAdjustPortalSideHandle::OnEndUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustHeightDoorHandle::OnEndUse"));

		if (Controller.IsValid() && MOI && Controller->EMPlayerState->DoesObjectHaveAnyError(MOI->ID))
		{
			return OnAbortUse();
		}
		else if (!FEditModelAdjustmentHandleBase::OnEndUse())
		{
			return false;
		}

		MOI->ShowAdjustmentHandles(Controller.Get(), true);

		// Console Command to get to undo/redo
		FVector newMoiActorLocation = UKismetMathLibrary::TransformLocation(MOI->GetActor()->GetActorTransform(), MOI->ControlPoints[3]);

		TArray<FVector> newPoints = MOI->ControlPoints;
		MOI->ControlPoints = BeginUpdateHolePonts;

		Controller->ModumateCommand(FModumateCommand(Commands::kUpdateMOIHoleParams)
			.Param(Parameters::kObjectID, MOI->ID)
			.Param(Parameters::kLocation, newMoiActorLocation)
			.Param(Parameters::kControlPoints, newPoints));

		return true;
	};

	bool FAdjustPortalSideHandle::OnAbortUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustHeightDoorHandle::OnAbortUse"));

		if (!FEditModelAdjustmentHandleBase::OnAbortUse())
		{
			return false;
		}

		MOI->ShowAdjustmentHandles(Controller.Get(), true);
		MOI->ControlPoints = BeginUpdateHolePonts;
		MOI->UpdateGeometry();

		auto *wallParent = MOI->GetParentObject();
		if (wallParent && wallParent->ObjectType == EObjectType::OTWallSegment)
		{
			wallParent->UpdateGeometry();
		}

		return true;
	};

	FVector FAdjustPortalSideHandle::GetAttachmentPoint()
	{
		//UE_LOG(LogCallTrace, Display, TEXT("AdjustHeightHandle::GetAttachmentPoint"));
		// Draw Dimension Strings
		if (Handle->GetStaticMeshComponent()->IsVisible())
		{
			Controller = Cast<AEditModelPlayerController_CPP>(MOI->GetActor()->GetWorld()->GetFirstPlayerController());
			bool bIsParentMoiSelected = false;
			if (Controller->DimStringWidgetSelectedObject)
			{
				if (Controller->DimStringWidgetSelectedObject == MOI->GetActor())
				{
					bIsParentMoiSelected = true;
				}
			}
			EEnterableField verticalTextBoxField = EEnterableField::EditableText_ImperialUnits_NoUserInput;
			EEnterableField horizontalTextBoxField = EEnterableField::EditableText_ImperialUnits_NoUserInput;

			if (bIsParentMoiSelected)
			{
				if (Controller->EnablePortalVerticalInput)
				{
					verticalTextBoxField = EEnterableField::EditableText_ImperialUnits_UserInput;
				}
				if (Controller->EnablePortalHorizontalInput)
				{
					horizontalTextBoxField = EEnterableField::EditableText_ImperialUnits_UserInput;
				}
			}

			if (Side == 1) // Horizontal Dim string text field, always on
			{
				FVector cpPortal1 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[1]);
				FVector cpPortal2 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[2]);
				FVector offsetDir = MOI->GetActor()->GetActorUpVector();
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller.Get(),
					cpPortal1,
					cpPortal2,
					offsetDir,
					MOI->GetActor()->GetFName(),
					Controller->DimensionStringUniqueID_PortalWidth,
					0,
					MOI->GetActor(),
					EDimStringStyle::Fixed,
					horizontalTextBoxField,
					40.f,
					EAutoEditableBox::Never,
					true);
			}
			if (Side == 0) // Vertical Dim string text field, always on
			{
				FVector cpPortal0 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[0]);
				FVector cpPortal1 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[1]);
				FVector offsetDir = MOI->GetActor()->GetActorForwardVector() * FVector(-1.f);
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller.Get(),
					cpPortal0,
					cpPortal1,
					offsetDir,
					MOI->GetActor()->GetFName(),
					Controller->DimensionStringUniqueID_PortalHeight,
					1,
					MOI->GetActor(),
					EDimStringStyle::Fixed,
					verticalTextBoxField,
					40.f,
					EAutoEditableBox::Never,
					true);
			}
		}

		if (MOI->GetActor() != nullptr && ((MOI->ObjectType == EObjectType::OTDoor) || (MOI->ObjectType == EObjectType::OTWindow)))
		{
			TArray<FVector> worldCPs;
			for (int32 i = 0; i < MOI->ControlPoints.Num(); ++i)
			{
				worldCPs.Add(UKismetMathLibrary::TransformLocation(MOI->GetActor()->GetActorTransform(), MOI->ControlPoints[i]));
			}
			// Side 0 = Right : 1 = Left : 2 = UP : 3 = DOWN
			switch (Side)
			{
			case 0:
				return (worldCPs[0] + worldCPs[1]) / 2.0f;
			case 1:
				return (worldCPs[2] + worldCPs[3]) / 2.0f;
			case 2:
				return (worldCPs[1] + worldCPs[2]) / 2.0f;
			case 3:
				return (worldCPs[0] + worldCPs[3]) / 2.0f;
			}

			return worldCPs[Side];
		}

		else
		{
			ensureAlways(false);
			return FVector::ZeroVector;
		}
	};

	bool FAdjustPortalSideHandle::HandleInputNumber(float number)
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustHeightDoorHandle::HandleInputNumber"));
		if (abs(number) > 1.0f)
		{
			if (Side == 0 || Side == 1)
			{
				int32 thisCpId = Side == 0 ? 0 : 3;
				int32 otherCpId = Side == 0 ? 3 : 0;
				FVector delta = (MOI->ControlPoints[thisCpId] - MOI->ControlPoints[otherCpId]).GetSafeNormal() * number;
				if (Controller->EnableDrawTotalLine && number > 0.f)
				{
					// this is to use delta as total length
					if (Side == 0)
					{
						MOI->ControlPoints[0] = MOI->ControlPoints[3] + delta;
						MOI->ControlPoints[1] = MOI->ControlPoints[2] + delta;
					}
					else
					{
						MOI->ControlPoints[2] = MOI->ControlPoints[1] + delta;
						MOI->ControlPoints[3] = MOI->ControlPoints[0] + delta;
					}
				}
				else if (Controller->EnableDrawDeltaLine)
				{
					bool bPositive = (MOI->ControlPoints[thisCpId] - MOI->ControlPoints[otherCpId]).Size() >= (BeginUpdateHolePonts[thisCpId] - BeginUpdateHolePonts[otherCpId]).Size();
					if (!bPositive)
					{
						delta = delta * -1.f;
					}
					// this is to add delta height
					if (Side == 0)
					{
						MOI->ControlPoints[0] = BeginUpdateHolePonts[0] + delta;
						MOI->ControlPoints[1] = BeginUpdateHolePonts[1] + delta;
					}
					else
					{
						MOI->ControlPoints[2] = BeginUpdateHolePonts[2] + delta;
						MOI->ControlPoints[3] = BeginUpdateHolePonts[3] + delta;
					}
				}
				OnEndUse();
				return true;
			}
			else if (Side == 2)
			{
				if (Controller->EnableDrawTotalLine && number > 0.f)
				{
					// this is use as total height
					MOI->ControlPoints[1].Z = MOI->ControlPoints[0].Z + number;
					MOI->ControlPoints[2].Z = MOI->ControlPoints[3].Z + number;
				}
				else if (Controller->EnableDrawDeltaLine)
				{
					// this is to add delta height
					bool bPositive = MOI->ControlPoints[1].Z >= BeginUpdateHolePonts[1].Z;
					float numberAdjusted = bPositive ? number : number * -1.f;
					MOI->ControlPoints[1].Z = BeginUpdateHolePonts[1].Z + numberAdjusted;
					MOI->ControlPoints[2].Z = BeginUpdateHolePonts[2].Z + numberAdjusted;
				}
				OnEndUse();
				return true;
			}
			else if (Side == 3)
			{
				if (Controller->EnableDrawTotalLine && number > 0.f)
				{
					// this is use as total height
					MOI->ControlPoints[0].Z = MOI->ControlPoints[1].Z - number;
					MOI->ControlPoints[3].Z = MOI->ControlPoints[2].Z - number;
				}
				else if (Controller->EnableDrawDeltaLine)
				{
					// this is to add delta height
					bool bPositive = MOI->ControlPoints[0].Z >= BeginUpdateHolePonts[0].Z;
					float numberAdjusted = bPositive ? number : number * -1.f;
					MOI->ControlPoints[0].Z = BeginUpdateHolePonts[0].Z + numberAdjusted;
					MOI->ControlPoints[3].Z = BeginUpdateHolePonts[3].Z + numberAdjusted;
				}
				OnEndUse();
				return true;
			}
		}
		return true;
	}

	bool FAdjustPortalInvertHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustDoorInvertHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		TArray<int32> ids = { MOI->ID };

		if (Sign > 0.f)
		{
			Controller->ModumateCommand(
				FModumateCommand(Commands::kInvertObjects)
				.Param(Parameters::kObjectIDs, ids));
		}
		else
		{
			Controller->ModumateCommand(
				FModumateCommand(Commands::kTransverseObjects)
				.Param(Parameters::kObjectIDs, ids));
		}
		OnEndUse();
		MOI->ShowAdjustmentHandles(Controller.Get(), true);
		return false;
	}

	FVector FAdjustPortalInvertHandle::GetAttachmentPoint()
	{
		FVector cpPortal0 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[0]);
		FVector cpPortal3 = MOI->GetActor()->GetActorTransform().TransformPosition(MOI->ControlPoints[3]);
		FVector cpMid = (cpPortal0 + cpPortal3) / 2.f;
		FVector cpMid0 = (cpPortal0 + (cpPortal0 + cpPortal3) / 2.f) / 2.f;
		FVector cpMid3 = (cpPortal3 + (cpPortal0 + cpPortal3) / 2.f) / 2.f;

		if (Sign > 0.f)
		{
			Handle->HandleDirection = (cpPortal3 - cpPortal0).GetSafeNormal();
			if (MOI->ObjectTransversed)
			{
				return cpMid3;
			}
			else
			{
				return cpMid0;
			}
		}
		else
		{
			FVector portalDir = (cpPortal3 - cpPortal0).GetSafeNormal();
			Handle->HandleDirection = UKismetMathLibrary::RotateAngleAxis(portalDir, 90.f, FVector::UpVector);
			if (MOI->ObjectTransversed)
			{
				return cpMid0;
			}
			else
			{
				return cpMid3;
			}
		}
	}

	bool FAdjustPortalPointHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyPointHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		OriginalLocHandle = GetAttachmentPoint();
		OrginalLoc = MOI->GetActor()->GetActorLocation();
		OriginalP = MOI->ControlPoints;
		FVector actorLoc = Handle->Implementation->GetAttachmentPoint();
		AnchorLoc = FVector(actorLoc.X, actorLoc.Y, MOI->GetActor()->GetActorLocation().Z);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector::UpVector);

		MOI->ShowAdjustmentHandles(Controller.Get(), false);

		return true;
	}

	bool FAdjustPortalPointHandle::OnUpdateUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyPointHandle::OnUpdateUse"));

		FEditModelAdjustmentHandleBase::OnUpdateUse();
		auto *wallParent = MOI->GetParentObject();
		bool bValidParentWall = wallParent && wallParent->ObjectType == EObjectType::OTWallSegment;

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
		FVector worldCP0 = UKismetMathLibrary::TransformLocation(MOI->GetActor()->GetActorTransform(), MOI->ControlPoints[0]);
		FVector worldCP3 = UKismetMathLibrary::TransformLocation(MOI->GetActor()->GetActorTransform(), MOI->ControlPoints[3]);

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
		bool cpEdgeCheck = UModumateFunctionLibrary::IsTargetCloseToControlPointEdges(hitIntersection, MOI->ControlPoints, MOI->GetActor()->GetActorTransform(), 10.f);
		if (Controller->EMPlayerState->SnappedCursor.Visible && !cpEdgeCheck)
		{
			switch (Controller->EMPlayerState->SnappedCursor.SnapType)
			{
			case ESnapType::CT_CORNERSNAP:
			case ESnapType::CT_MIDSNAP:
			case ESnapType::CT_EDGESNAP:
			case ESnapType::CT_FACESELECT:
			{
				FVector lineDirection = MOI->GetObjectRotation().Vector();
				hitIntersection = UKismetMathLibrary::FindClosestPointOnLine(hitPoint, hitIntersection, lineDirection);

				// Draw affordance lines if there is snapping
				FAffordanceLine affordanceH;
				affordanceH.Color = FLinearColor(FColorList::Black);
				affordanceH.EndPoint = FVector(hitPoint.X, hitPoint.Y, GetAttachmentPoint().Z);
				affordanceH.StartPoint = GetAttachmentPoint();
				affordanceH.Interval = 4.0f;
				Controller->EMPlayerState->AffordanceLines.Add(affordanceH);
				FAffordanceLine affordanceV;
				affordanceV.Color = FLinearColor::Blue;
				affordanceV.EndPoint = hitPoint;
				affordanceV.StartPoint = FVector(hitPoint.X, hitPoint.Y, GetAttachmentPoint().Z);
				affordanceV.Interval = 4.0f;
				Controller->EMPlayerState->AffordanceLines.Add(affordanceV);
				break;
			}
			default: break;
			}
		}

		if (MOI->ObjectType == EObjectType::OTDoor) // Doors cannot move vertically, limit movement to control point 0 and 1
		{
			hitIntersection.Z = worldCP0.Z;
			FVector moveDiff;
			if (HandleID == 0 || HandleID == 1 || HandleID == 4 || HandleID == 5)
			{
				moveDiff = MOI->ControlPoints[0] - UKismetMathLibrary::InverseTransformLocation(MOI->GetActor()->GetActorTransform(), hitIntersection);
			}
			else
			{
				moveDiff = MOI->ControlPoints[3] - UKismetMathLibrary::InverseTransformLocation(MOI->GetActor()->GetActorTransform(), hitIntersection);
			}
			MOI->ControlPoints[0].X -= moveDiff.X;
			MOI->ControlPoints[1].X -= moveDiff.X;
			MOI->ControlPoints[2].X -= moveDiff.X;
			MOI->ControlPoints[3].X -= moveDiff.X;
		}
		else
		{
			FVector moveDiff;
			int32 handleCPConvert = HandleID % 4;
			moveDiff = MOI->ControlPoints[handleCPConvert] - UKismetMathLibrary::InverseTransformLocation(MOI->GetActor()->GetActorTransform(), hitIntersection);
			MOI->ControlPoints[0].X -= moveDiff.X;
			MOI->ControlPoints[1].X -= moveDiff.X;
			MOI->ControlPoints[2].X -= moveDiff.X;
			MOI->ControlPoints[3].X -= moveDiff.X;
			//Replace with below if want to freely move vertically
			//MOI->ControlPoints[0] -= moveDiff;
			//MOI->ControlPoints[1] -= moveDiff;
			//MOI->ControlPoints[2] -= moveDiff;
			//MOI->ControlPoints[3] -= moveDiff;
		}
		// Point horizontal dim string. Delta only
		FVector offsetDir = MOI->GetActor()->GetActorRightVector();
		UModumateFunctionLibrary::AddNewDimensionString(
			Controller.Get(),
			OriginalLocHandle,
			GetAttachmentPoint(),
			offsetDir,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			MOI->GetActor(),
			EDimStringStyle::HCamera);

		MOI->UpdateGeometry();
		if (bValidParentWall)
		{
			wallParent->UpdateGeometry();
		}

		return true;
	}

	bool FAdjustPortalPointHandle::OnEndUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnEndUse())
		{
			return false;
		}

		TArray<FVector> newCPs = MOI->ControlPoints;
		MOI->ControlPoints = OriginalP;

		Controller->ModumateCommand(
			FModumateCommand(Commands::kUpdateMOIHoleParams)
			.Param(Parameters::kObjectID, MOI->ID)
			.Param(Parameters::kLocation, MOI->GetObjectLocation())
			.Param(Parameters::kControlPoints, newCPs));

		MOI->ShowAdjustmentHandles(Controller.Get(), true);

		return true;
	}

	bool FAdjustPortalPointHandle::OnAbortUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnAbortUse())
		{
			return false;
		}

		MOI->ControlPoints = OriginalP;
		MOI->UpdateGeometry();
		auto *wallParent = MOI->GetParentObject();
		bool bValidParentWall = wallParent && wallParent->ObjectType == EObjectType::OTWallSegment;
		if (bValidParentWall)
		{
			wallParent->UpdateGeometry();
		}

		return true;
	}

	FVector FAdjustPortalPointHandle::GetAttachmentPoint()
	{
		FVector attachPoint;
		float parentWallThickness = 0.f;
		auto *wallParent = MOI->GetParentObject();
		if (wallParent == nullptr)
		{
			return FVector::ZeroVector;
		}
		FVector portalSideDir = MOI->GetNormal();
		if (wallParent && wallParent->ObjectType == EObjectType::OTWallSegment)
		{
			parentWallThickness = wallParent->CalculateThickness();
		}

		// Get location of handles regardless which side of the parent wall
		int32 cpIndex = HandleID % 4;
		FTransform portalTransform = MOI->GetActor()->GetActorTransform();
		attachPoint = portalTransform.TransformPosition(MOI->ControlPoints[cpIndex]);

		APlayerCameraManager* cameraActor = UGameplayStatics::GetPlayerCameraManager(Handle.Get(), 0);
		FVector cameraLoc = cameraActor->GetCameraLocation();
		FVector otherAttachPoint = attachPoint - (portalSideDir * parentWallThickness);
		bool bHideOtherAttachPoint = FVector::Dist(attachPoint, cameraLoc) < FVector::Dist(otherAttachPoint, cameraLoc);

		if (HandleID < 4)
		{
			Handle->SetActorHiddenInGame(!bHideOtherAttachPoint);
			return attachPoint;
		}
		else
		{
			Handle->SetActorHiddenInGame(bHideOtherAttachPoint);
			return otherAttachPoint;
		}
	}

	bool FAdjustPortalPointHandle::HandleInputNumber(float number)
	{
		FVector currentDirection = (MOI->ControlPoints[0] - OriginalP[0]).GetSafeNormal();

		TArray<FVector> newCPs;
		for (int32 i = 0; i < OriginalP.Num(); ++i)
		{
			newCPs.Add(OriginalP[i] + (currentDirection * number));
		}
		MOI->ControlPoints = OriginalP;

		Controller->ModumateCommand(
			FModumateCommand(Commands::kUpdateMOIHoleParams)
			.Param(Parameters::kObjectID, MOI->ID)
			.Param(Parameters::kLocation, MOI->GetObjectLocation())
			.Param(Parameters::kControlPoints, newCPs));

		OnEndUse();
		return true;
	}
}
