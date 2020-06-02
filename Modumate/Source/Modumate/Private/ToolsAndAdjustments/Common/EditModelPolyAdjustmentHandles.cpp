// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/EditModelPolyAdjustmentHandles.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Algo/Accumulate.h"

namespace Modumate
{
	bool FAdjustPolyPointHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyPointHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		OriginalP.Empty();
		for (int32 cp : CP)
		{
			OriginalP.Add(MOI->GetControlPoint(cp));
			LastValidPendingCPLocations.Add(MOI->GetControlPoint(cp));
		}

		FPlane polyPlane(MOI->GetControlPoint(0), MOI->GetControlPoint(1), MOI->GetControlPoint(2));
		AnchorLoc = FVector::PointPlaneProject(Handle->Implementation->GetAttachmentPoint(), polyPlane);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, FVector(polyPlane));

		MOI->ShowAdjustmentHandles(Controller.Get(), false);

		// Tell mesh it is being edited at handle
		ADynamicMeshActor* dynMeshActor = Cast<ADynamicMeshActor>(MOI->GetActor());
		if (dynMeshActor != nullptr)
		{
			dynMeshActor->AdjustHandleFloor = CP;
		}
		return true;
	}

	bool FAdjustPolyPointHandle::OnUpdateUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyPointHandle::OnUpdateUse"));

		FEditModelAdjustmentHandleBase::OnUpdateUse();

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
				int32 numTotalCPs = MOI->GetControlPoints().Num();
				FVector closestPoint1, closestPoint2;
				FVector currentEdgeDirection = (MOI->GetControlPoint(CP[1]) - MOI->GetControlPoint(CP[0])).GetSafeNormal();

				// Intersection test for first CP
				int32 startID0 = (CP[0] + numTotalCPs - 1) % numTotalCPs;
				int32 endID0 = CP[0];
				FVector lineDirection0 = (MOI->GetControlPoint(endID0) - MOI->GetControlPoint(startID0)).GetSafeNormal();
				bool b1 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, hitPoint, currentEdgeDirection, MOI->GetControlPoint(endID0), lineDirection0);
				FVector newCP0 = closestPoint1;

				// Intersection test for second CP
				int32 startID1 = (CP[1] + 1) % numTotalCPs;
				int32 endID1 = CP[1];
				FVector lineDirection1 = (MOI->GetControlPoint(endID1) - MOI->GetControlPoint(startID1)).GetSafeNormal();
				bool b2 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, hitPoint, currentEdgeDirection, MOI->GetControlPoint(endID1), lineDirection1);
				FVector newCP1 = closestPoint1;

				// Set MOI control points to new CP
				MOI->SetControlPoint(CP[0],newCP0);
				MOI->SetControlPoint(CP[1],newCP1);

				// Check if CPs are intersect. If it is, don't update geometry
				// Store last good locations to fall back on if the new floor lacks proper triangulation
				if (UModumateGeometryStatics::ArePolygonEdgesValid(MOI->GetControlPoints()))
				{
					UpdateTargetGeometry();
					LastValidPendingCPLocations = { MOI->GetControlPoint(CP[0]) , MOI->GetControlPoint(CP[1]) };
				}
				else
				{
					MOI->SetControlPoint(CP[0],LastValidPendingCPLocations[0]);
					MOI->SetControlPoint(CP[1],LastValidPendingCPLocations[1]);
				}

				HandleOriginalPoint = (OriginalP[0] + OriginalP[1]) / 2.f;
				FVector curSideMidLoc = (MOI->GetControlPoint(CP[0]) + MOI->GetControlPoint(CP[1])) / 2.f;
				currentEdgeDirection = (MOI->GetControlPoint(CP[1]) - MOI->GetControlPoint(CP[0])).GetSafeNormal();
				bool b3 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, HandleOriginalPoint, Handle->HandleDirection, curSideMidLoc, currentEdgeDirection);
				HandleCurrentPoint = closestPoint1;

				// Dim string. between cursor pos and original handle pos. Delta only
				UModumateFunctionLibrary::AddNewDimensionString(Controller.Get(),
					HandleOriginalPoint,
					HandleCurrentPoint,
					currentEdgeDirection,
					Controller->DimensionStringGroupID_Default,
					Controller->DimensionStringUniqueID_Delta,
					0,
					MOI->GetActor(),
					EDimStringStyle::HCamera,
					EEnterableField::NonEditableText,
					20.f,
					EAutoEditableBox::UponUserInput_SameGroupIndex,
					true);

				// Show dim string from SideCPs
				bool bIsParallel = Handle->ShowSideCPParallelDimensionString(true, 1);
				if (!bIsParallel)
				{
					Handle->ShowSideCPDimensionString(0, true, 1);
					Handle->ShowSideCPDimensionString(1, true, 2);
				}
			}
			else if (CP.Num() == 1)
			{
				MOI->SetControlPoint(CP[0],OriginalP[0] + dp);
				if (UModumateGeometryStatics::ArePolygonEdgesValid(MOI->GetControlPoints()))
				{
					UpdateTargetGeometry();
					LastValidPendingCPLocations = { MOI->GetControlPoint(CP[0]) };
				}
				else
				{
					MOI->SetControlPoint(CP[0],LastValidPendingCPLocations[0]);
				}

				FVector offsetDir = dp.GetSafeNormal() ^ FVector(Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

				// Dim string between cursor pos and original point handle position. Delta only
				UModumateFunctionLibrary::AddNewDimensionString(Controller.Get(),
					OriginalP[0],
					MOI->GetControlPoint(CP[0]),
					offsetDir,
					Controller->DimensionStringGroupID_Default,
					Controller->DimensionStringUniqueID_Delta,
					0,
					MOI->GetActor(),
					EDimStringStyle::HCamera,
					EEnterableField::NonEditableText,
					40.f,
					EAutoEditableBox::Never);

				// Show dim string next to the current handle
				Handle->ShowHoverFloorDimensionString(false);
			}
		}
		return true;
	}

	FVector FAdjustPolyPointHandle::GetAttachmentPoint()
	{
		if (!ensure(MOI))
		{
			return FVector::ZeroVector;
		}

		int32 numTargetCP = CP.Num();
		FVector averageTargetPos(ForceInitToZero);
		for (int32 cpIndex : CP)
		{
			if (!ensure(MOI->GetControlPoints().IsValidIndex(cpIndex)))
			{
				return FVector::ZeroVector;
			}

			averageTargetPos += MOI->GetControlPoint(cpIndex);
		}
		averageTargetPos /= numTargetCP;

		if (MOI->GetObjectType() == EObjectType::OTMetaPlane || MOI->GetObjectType() == EObjectType::OTCutPlane)
		{
			return averageTargetPos;
		}
		else if (MOI->GetObjectType() == EObjectType::OTScopeBox)
		{
			return MOI->GetObjectRotation().RotateVector(MOI->GetNormal() * 0.5f * MOI->GetExtents().Y + averageTargetPos);
		}
		else
		{
			return MOI->GetObjectRotation().RotateVector(FVector(0, 0, 0.5f * MOI->GetExtents().Y) + averageTargetPos);
		}
	}

	bool FAdjustPolyPointHandle::HandleInputNumber(float number)
	{
		// Handle should only affect two control points
		if (CP.Num() == 2)
		{
			TArray<FVector> proxyCPs = MOI->GetControlPoints();
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
					FVector preCP0 = MOI->GetControlPoint((CP[0] + proxyCPs.Num() - 1) % proxyCPs.Num());
					FVector postCP1 = MOI->GetControlPoint((CP[1] + 1) % proxyCPs.Num());
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
				FVector lineDirection0 = UKismetMathLibrary::GetDirectionUnitVector(MOI->GetControlPoint(startID0), MOI->GetControlPoint(endID0));
				bool b1 = UModumateFunctionLibrary::ClosestPointsOnTwoLines(closestPoint1, closestPoint2, inputHandleLocation, currentEdgeDirection, OriginalP[0], lineDirection0);
				newCP0 = FVector(closestPoint1.X, closestPoint1.Y, closestPoint1.Z);

				// Intersection test for second CP
				int32 startID1 = (CP[1] + 1) % proxyCPs.Num();
				int32 endID1 = CP[1];
				FVector lineDirection1 = UKismetMathLibrary::GetDirectionUnitVector(MOI->GetControlPoint(startID1), MOI->GetControlPoint(endID1));
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
				FVector preCP0 = MOI->GetControlPoint((CP[0] + proxyCPs.Num() - 1) % proxyCPs.Num());
				FVector postCP1 = MOI->GetControlPoint((CP[1] + 1) % proxyCPs.Num());
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
				MOI->SetControlPoint(CP[0],newCP0);
				MOI->SetControlPoint(CP[1],newCP1);
				return OnEndUse();
			}
			else
			{
				// The result triangulation will create invalid geometry
				return false;
			}
		}
		return false; // false by default if not handling numbers
	}

	bool FAdjustPolyExtrusionHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyExtrusionHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		if (!MOI || (MOI->GetObjectType() != EObjectType::OTCabinet && MOI->GetObjectType() != EObjectType::OTScopeBox))
		{
			UE_LOG(LogCallTrace, Error, TEXT("AdjustPolyExtrusionHandle only currently supported for Cabinets!"));
			return false;
		}

		OriginalControlPoints = MOI->GetControlPoints();
		OriginalPlane = FPlane(MOI->GetControlPoints().Num() > 0 ? MOI->GetControlPoint(0) : MOI->GetObjectLocation(), MOI->GetNormal());
		OriginalExtrusion = MOI->GetExtents().Y;
		LastValidExtrusion = OriginalExtrusion;
		AnchorLoc = Handle->Implementation->GetAttachmentPoint();

		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, MOI->GetNormal());

		MOI->ShowAdjustmentHandles(Controller.Get(), false);

		return true;
	}

	bool FAdjustPolyExtrusionHandle::OnUpdateUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyExtrusionHandle::OnUpdateUse"));

		FEditModelAdjustmentHandleBase::OnUpdateUse();

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
				FVector extents = MOI->GetExtents();
				extents.Y = newExtrusion;
				MOI->SetExtents(extents);

				LastValidExtrusion = newExtrusion;

				if (Sign < 0.0f)
				{
					for (int32 i = 0; i < MOI->GetControlPoints().Num(); ++i)
					{
						MOI->SetControlPoint(i,OriginalControlPoints[i] - (OriginalPlane * extrusionDelta));
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
			Controller.Get(),
			AnchorLoc,
			GetAttachmentPoint(),
			camDir,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			MOI->GetActor(),
			EDimStringStyle::Fixed);

		FVector bottomStringLoc = FVector(GetAttachmentPoint().X, GetAttachmentPoint().Y, MOI->GetControlPoint(0).Z);
		FVector topStringLoc = bottomStringLoc + FVector(0.f, 0.f, MOI->GetExtents().Y);

		// Dim string for vertical extrusion handle - Total
		UModumateFunctionLibrary::AddNewDimensionString(
			Controller.Get(),
			bottomStringLoc,
			topStringLoc,
			camDir,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Total,
			1,
			MOI->GetActor(),
			EDimStringStyle::Fixed);
		return true;
	}

	FVector FAdjustPolyExtrusionHandle::GetAttachmentPoint()
	{
		if (MOI->GetControlPoints().Num() > 0)
		{
			FVector objectNormal = MOI->GetNormal();
			FVector offset = (Sign > 0.0f) ? (MOI->GetExtents().Y * objectNormal) : FVector::ZeroVector;
			FVector centroid = Algo::Accumulate(
				MOI->GetControlPoints(), 
				FVector::ZeroVector,
				[](FVector centroid, const FVector &p) { return centroid + p; }) / MOI->GetControlPoints().Num();

			return offset + centroid;
		}
		else
		{
			return MOI->GetObjectLocation();
		}
	}

	bool FAdjustPolyExtrusionHandle::HandleInputNumber(float number)
	{
		TArray<FVector> newPoints;
		FVector newExtents(ForceInitToZero);

		if (Controller->EMPlayerState->CurrentDimensionStringWithInputUniqueID == Controller->DimensionStringUniqueID_Delta)
		{
			float dir = FMath::Sign(MOI->GetExtents().Y - OriginalExtrusion);
			if (Sign > 0.f)
			{
				newPoints = MOI->GetControlPoints();
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
			newPoints = MOI->GetControlPoints();
		}

		// Tell mesh it is done editing at handle
		ADynamicMeshActor* dynMeshActor = Cast<ADynamicMeshActor>(MOI->GetActor());
		if (dynMeshActor != nullptr)
		{
			dynMeshActor->AdjustHandleFloor.Empty();
		}

		MOI->SetControlPoints(newPoints);
		MOI->SetExtents(newExtents);
		return FEditModelAdjustmentHandleBase::OnEndUse();
	}

	bool FAdjustInvertHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustInvertHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		MOI->SetObjectInverted(!MOI->GetObjectInverted());

		OnEndUse();
		return false;
	}

	FVector FAdjustInvertHandle::GetAttachmentPoint()
	{
		if (ensure(Handle.IsValid()))
		{
			FVector attachLocation = MOI->GetObjectLocation();
			float camDistance = (Handle->GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation() - attachLocation).Size();
			static const float handleOffsetMultiplier = -0.025f;
			return attachLocation + (camDistance * handleOffsetMultiplier * FVector::UpVector);
		}
		return FVector::ZeroVector;
	}

	bool FJustificationHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FJustificationHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		if (Handle->HandleChildren.Num() > 0 && !Handle->bShowHandleChildren)
		{
			Handle->bShowHandleChildren = true;
			for (auto& childHandle : Handle->HandleChildren)
			{
				childHandle->SetEnabled(Handle->bShowHandleChildren);
			}

			if (Handle->DynamicMaterial != nullptr)
			{
				float showExpandAlpha = Handle->bShowHandleChildren ? 1.f : 0.f;
				static const FName expandParamName(TEXT("ShowExpand"));
				Handle->DynamicMaterial->SetScalarParameterValue(expandParamName, showExpandAlpha);
			}
		}
		else
		{
			MOI->SetExtents(FVector(LayerPercentage, 0.0f, 0.0f));
		}

		OnEndUse();
		return false;
	}

	FVector FJustificationHandle::GetAttachmentPoint()
	{
		// Attachment point of justification handle is offset by screen size
		// This value will affect the scaling of the offset
		const float relativeCameraScale = .065f;

		FVector attachLocation = FVector::ZeroVector;
		if (Controller.IsValid())
		{
			// Calculate the world location of justification handles
			// Origin is the metaplane's center location
			// Handles are offset from origin by the amount in LayerPercentage
			FVector originLocation = MOI->GetObjectLocation();
			float camDistanceScaled = (Controller->PlayerCameraManager->GetCameraLocation() - originLocation).Size() * relativeCameraScale;

			FVector objectNormal = MOI->GetNormal();
			FVector worldP0 = originLocation + (objectNormal * camDistanceScaled);
			FVector worldP1 = originLocation + (objectNormal * camDistanceScaled * -1.f);

			attachLocation = FMath::Lerp(worldP1, worldP0, LayerPercentage);
		}
		return attachLocation;
	}
}
