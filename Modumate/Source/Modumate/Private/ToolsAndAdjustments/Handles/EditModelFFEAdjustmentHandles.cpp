// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelFFEAdjustmentHandles.h"
#include "Algo/Accumulate.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateGeometryStatics.h"
#include "ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "ModumateCommands.h"

namespace Modumate
{
	FAdjustFFEPointHandle::FAdjustFFEPointHandle(FModumateObjectInstance *moi, int32 cp)
		: FEditModelAdjustmentHandleBase(moi)
		, HandleID(cp)
		, AssemblyNormal(FVector::UpVector)
		, AssemblyTangent(FVector::RightVector)
		, LocalHandlePos(ForceInitToZero)
	{
		bool bHaveMountingProperties =
			MOI->GetAssembly().TryGetProperty(BIM::Parameters::Normal, AssemblyNormal) &&
			MOI->GetAssembly().TryGetProperty(BIM::Parameters::Tangent, AssemblyTangent);
		ensure(bHaveMountingProperties);

		AActor *moiActor = moi->GetActor();

		TArray<FVector> boxSidePoints;
		if (ensure(moiActor && (HandleID >= 0) && (HandleID < 4)) &&
			UModumateObjectStatics::GetFFEBoxSidePoints(moiActor, AssemblyNormal, boxSidePoints))
		{
			FVector actorLoc = moiActor->GetActorLocation();
			FQuat actorRot = moiActor->GetActorQuat();
			FVector curObjectNormal = actorRot.RotateVector(AssemblyNormal);
			FVector handleCornerPos = boxSidePoints[HandleID];
			FVector curHandlePos = FVector::PointPlaneProject(handleCornerPos, actorLoc, curObjectNormal);
			LocalHandlePos = moiActor->GetActorTransform().InverseTransformPosition(curHandlePos);
		}
	}

	bool FAdjustFFEPointHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyPointHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		FVector actorLoc = Handle->Implementation->GetAttachmentPoint();
		AnchorLoc = FVector(actorLoc.X, actorLoc.Y, MOI->GetActor()->GetActorLocation().Z);

		OriginalHandleLoc = GetAttachmentPoint();
		OriginalObjectLoc = MOI->GetActor()->GetActorLocation();
		OriginalObjectRot = MOI->GetObjectRotation();

		FVector curObjectNormal = OriginalObjectRot.RotateVector(AssemblyNormal);
		FVector curObjectTangent = OriginalObjectRot.RotateVector(AssemblyTangent);

		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(OriginalHandleLoc, curObjectNormal, curObjectTangent);

		MOI->ShowAdjustmentHandles(Controller.Get(), false);

		return true;
	}

	bool FAdjustFFEPointHandle::OnUpdateUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustPolyPointHandle::OnUpdateUse"));

		FEditModelAdjustmentHandleBase::OnUpdateUse();

		if (!UpdateTarget())
		{
			return false;
		}

		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		FTransform newTransform = MOI->GetActor()->GetActorTransform();
		FVector translation = newTransform.GetLocation() - OriginalObjectLoc;

		// If the object has moved along its existing mounting plane, then show dimension strings
		if (newTransform.GetRotation().Equals(OriginalObjectRot) &&
			FMath::IsNearlyZero(translation | cursor.AffordanceFrame.Normal, PLANAR_DOT_EPSILON))
		{
			FVector newHandleLoc = GetAttachmentPoint();

			FVector axisTangent = cursor.AffordanceFrame.Tangent;
			FVector axisBitangent = cursor.AffordanceFrame.Tangent ^ cursor.AffordanceFrame.Normal;

			FVector offsetTangent = translation.ProjectOnToNormal(axisTangent);
			FVector offsetBitangent = translation.ProjectOnToNormal(axisBitangent);

			// Add tangent dimension string (repurposing the Delta ID)
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				newHandleLoc - offsetTangent,
				newHandleLoc,
				axisBitangent,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Delta,
				0,
				MOI->GetActor(),
				EDimStringStyle::HCamera);

			// Add bitangent dimension string (repurposing the Total ID)
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller.Get(),
				newHandleLoc - offsetBitangent,
				newHandleLoc,
				axisTangent,
				Controller->DimensionStringGroupID_Default,
				Controller->DimensionStringUniqueID_Total,
				1,
				MOI->GetActor(),
				EDimStringStyle::Fixed);
		}

		return true;
	}

	bool FAdjustFFEPointHandle::UpdateTarget()
	{
		AActor *moiActor = MOI->GetActor();
		if (moiActor == nullptr)
		{
			return false;
		}

		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		if (!cursor.Visible)
		{
			return false;
		}

		FTransform newTransform = moiActor->GetActorTransform();

		// If we have a hit normal, then compute a mounting transform based on the cursor.
		if (UModumateObjectStatics::GetFFEMountedTransform(&MOI->GetAssembly(), cursor, newTransform))
		{
			// If we hit a face that isn't coincident with the original mounting plane,
			// then use the new mounting transform's rotation, as if we're placing the FF&E for the first time.
			if (FVector::Coincident(cursor.HitNormal, cursor.AffordanceFrame.Normal))
			{
				newTransform.SetRotation(OriginalObjectRot);
			}

			FVector newHandleOffset = newTransform.TransformVector(LocalHandlePos);
			FVector newHandlePlanarOffset = FVector::VectorPlaneProject(newHandleOffset, cursor.HitNormal);
			FVector newActorPos = newTransform.GetLocation() - newHandlePlanarOffset;

			newTransform.SetLocation(newActorPos);
		}
		// Otherwise, just translate the FF&E along the sketch plane
		else
		{
			FVector translation = (cursor.SketchPlaneProject(cursor.WorldPosition) - OriginalHandleLoc);
			newTransform.SetLocation(OriginalObjectLoc + translation);
			newTransform.SetRotation(OriginalObjectRot);
		}

		moiActor->SetActorTransform(newTransform);

		return true;
	}

	bool FAdjustFFEPointHandle::OnEndUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnEndUse())
		{
			return false;
		}

		AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
		if (moiActor == nullptr)
		{
			return false;
		}

		// Update the target now, in case the cursor has changed since the last UpdateUse (i.e. during HandleInputNumber)
		UpdateTarget();
		FTransform newTransform = MOI->GetActor()->GetActorTransform();

		MOI->ShowAdjustmentHandles(Controller.Get(), true);
		moiActor->SetActorTransform(FTransform(OriginalObjectRot, OriginalObjectLoc));

		// TODO: when FF&E can be properly parented, set kParentIDs to the new target.
		auto results = Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kSetObjectTransforms)
			.Param(Parameters::kObjectIDs, TArray<int32>({ MOI->ID }))
			.Param(Parameters::kParentIDs, TArray<int32>({ MOI->GetParentID() }))
			.Param(Parameters::kLocation, TArray<FVector>({ newTransform.GetLocation() }))
			.Param(Parameters::kQuaternion, TArray<FQuat>({ newTransform.GetRotation() }))
		);

		return results.GetValue(Parameters::kSuccess);
	}

	bool FAdjustFFEPointHandle::OnAbortUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnAbortUse())
		{
			return false;
		}

		AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
		if (moiActor)
		{
			MOI->ShowAdjustmentHandles(Controller.Get(), true);
			moiActor->SetActorTransform(FTransform(OriginalObjectRot, OriginalObjectLoc));
		}

		return true;
	}

	FVector FAdjustFFEPointHandle::GetAttachmentPoint()
	{
		AActor *moiActor = MOI->GetActor();
		if (!ensure(moiActor != nullptr))
		{
			return FVector::ZeroVector;
		}

		return moiActor->GetActorTransform().TransformPosition(LocalHandlePos);
	}

	bool FAdjustFFEPointHandle::HandleInputNumber(float number)
	{
		AActor *moiActor = MOI ? MOI->GetActor() : nullptr;
		if ((moiActor == nullptr) || !Controller.IsValid())
		{
			return false;
		}

		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		if (!cursor.Visible)
		{
			return true;
		}

		FTransform curTransform = moiActor->GetActorTransform();
		FVector curTranslation = curTransform.GetLocation() - OriginalObjectLoc;
		FQuat curRotation = curTransform.GetRotation();

		// If the object has moved off of its original mounting plane, don't handle the input numbers
		if (!curRotation.Equals(OriginalObjectRot) ||
			!FMath::IsNearlyZero(curTranslation | cursor.AffordanceFrame.Normal, PLANAR_DOT_EPSILON))
		{
			return false;
		}

		// Delta ID was used for tangent
		FVector moveAxis(ForceInitToZero);
		if (Controller->EnableDrawDeltaLine)
		{
			moveAxis = cursor.AffordanceFrame.Tangent;
		}
		// Total ID was used for bitangent
		else if (Controller->EnableDrawTotalLine)
		{
			moveAxis = cursor.AffordanceFrame.Tangent ^ cursor.AffordanceFrame.Normal;
		}
		else
		{
			return false;
		}

		if ((moveAxis | curTranslation) < 0.0f)
		{
			moveAxis *= -1.0f;
		}

		TArray<int32> objectIDs = { MOI->ID };
		FVector delta = number * moveAxis;

		moiActor->SetActorLocation(OriginalObjectLoc);
		Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kMoveObjects)
			.Param(Parameters::kObjectIDs, objectIDs)
			.Param(Parameters::kDelta, delta));

		OnEndUse();
		return true;
	}


	FAdjustFFERotateHandle::FAdjustFFERotateHandle(FModumateObjectInstance *moi, float sign)
		: FEditModelAdjustmentHandleBase(moi)
		, Sign(sign)
	{
		bool bHaveMountingProperties =
			MOI->GetAssembly().TryGetProperty(BIM::Parameters::Normal, AssemblyNormal) &&
			MOI->GetAssembly().TryGetProperty(BIM::Parameters::Tangent, AssemblyTangent);
		ensure(bHaveMountingProperties);

		AActor *moiActor = moi->GetActor();

		TArray<FVector> boxSidePoints;
		if (ensure(moiActor) &&
			UModumateObjectStatics::GetFFEBoxSidePoints(moiActor, AssemblyNormal, boxSidePoints))
		{
			FVector actorLoc = moiActor->GetActorLocation();
			FQuat actorRot = moiActor->GetActorQuat();
			FVector curObjectNormal = actorRot.RotateVector(AssemblyNormal);
			FVector averageBoxSidePos = Algo::Accumulate(boxSidePoints, FVector::ZeroVector) / boxSidePoints.Num();
			FVector curHandlePos = FVector::PointPlaneProject(averageBoxSidePos, actorLoc, curObjectNormal);
			LocalHandlePos = moiActor->GetActorTransform().InverseTransformPosition(curHandlePos);
		}
	}

	bool FAdjustFFERotateHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustFFERotateHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}
		AnchorLoc = MOI->GetObjectLocation();
		OriginalRotation = MOI->GetObjectRotation();

		FVector curObjectNormal = OriginalRotation.RotateVector(AssemblyNormal);
		FVector curObjectTangent = OriginalRotation.RotateVector(AssemblyTangent);

		// Choose one of the two for defining AnchorDirLocation
		//AnchorDirection = GetPlaneCursorHit(AnchorLoc, mousex, mousey); // #1: Use mouse pos to set AnchorDirLocation
		AnchorDirLocation = AnchorLoc + (curObjectTangent * 200.f); // #2: Use object rotation to set AnchorDirLocation

		// Setup location for world axis snap
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, curObjectNormal);

		MOI->ShowAdjustmentHandles(Controller.Get(), false);
		return true;
	}

	bool FAdjustFFERotateHandle::OnUpdateUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustFFERotateHandle::OnUpdateUse"));

		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		FVector projectedCursorPos = cursor.SketchPlaneProject(cursor.WorldPosition);

		FEditModelAdjustmentHandleBase::OnUpdateUse();
		if (Controller->EMPlayerState->SnappedCursor.Visible)
		{
			FQuat originalQuat = OriginalRotation;
			FQuat userQuat = GetAnchorQuatFromCursor();
			FVector userAxis;
			float userAngle;
			userQuat.ToAxisAndAngle(userAxis, userAngle);
			bClockwise = ((userAxis | cursor.AffordanceFrame.Normal) > 0.0f);
			FQuat quatRot = originalQuat * userQuat;

			MOI->SetObjectRotation(quatRot);

			// Start to draw dim string when object has been rotated
			if (!MOI->GetObjectRotation().Equals(OriginalRotation, 0.1f))
			{
				UModumateFunctionLibrary::AddNewDegreeString(
					Controller.Get(),
					AnchorLoc, //degree location
					AnchorDirLocation, // start
					projectedCursorPos, // end
					bClockwise,
					Controller->DimensionStringGroupID_PlayerController, // ControllerGroup
					Controller->DimensionStringUniqueID_Delta,
					0,
					Controller.Get(),
					EDimStringStyle::DegreeString,
					EEnterableField::NonEditableText,
					EAutoEditableBox::UponUserInput,
					true);

				// Line dim strings needs to be separate group from degree string to prevent TAB switching
				FName rotateDimLineGroup = FName(TEXT("DimLine"));

				// Dim string from AnchorLoc to cursor
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller.Get(),
					AnchorLoc,
					projectedCursorPos,
					FVector::ZeroVector,
					rotateDimLineGroup,
					Controller->DimensionStringUniqueID_Delta,
					1,
					Controller.Get(),
					EDimStringStyle::RotateToolLine,
					EEnterableField::None,
					40.f,
					EAutoEditableBox::Never,
					true,
					FColor::White);

				// Dim string from AnchorLoc to AnchorAngle
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller.Get(),
					AnchorLoc,
					AnchorDirLocation,
					FVector::ZeroVector,
					rotateDimLineGroup,
					Controller->DimensionStringUniqueID_Delta,
					1,
					Controller.Get(),
					EDimStringStyle::RotateToolLine,
					EEnterableField::None,
					40.f,
					EAutoEditableBox::Never,
					true,
					FColor::Green);
			}

		}
		return true;
	}

	bool FAdjustFFERotateHandle::OnEndUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnEndUse())
		{
			return false;
		}

		FQuat userQuat = GetAnchorQuatFromCursor();
		FRotator RotDir = userQuat.Rotator();

		MOI->SetObjectRotation(OriginalRotation);
		TArray<int32> objectIDs = { MOI->ID };
		Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kRotateObjects)
			.Param(Parameters::kObjectIDs, objectIDs)
			.Param(Parameters::kOrigin, MOI->GetObjectLocation())
			.Param(Parameters::kQuaternion, RotDir.Quaternion()));

		MOI->ShowAdjustmentHandles(Controller.Get(), true);
		return true;
	}

	bool FAdjustFFERotateHandle::OnAbortUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnAbortUse())
		{
			return false;
		}
		MOI->SetObjectRotation(OriginalRotation);
		MOI->ShowAdjustmentHandles(Controller.Get(), true);
		return true;
	}

	FVector FAdjustFFERotateHandle::GetAttachmentPoint()
	{
		AActor *moiActor = MOI->GetActor();
		if (!ensure(moiActor != nullptr))
		{
			return FVector::ZeroVector;
		}

		return moiActor->GetActorTransform().TransformPosition(LocalHandlePos);
	}

	bool FAdjustFFERotateHandle::GetOverrideHandleRotation(FQuat &OutRotation)
	{
		AActor *moiActor = MOI->GetActor();
		if (!ensure(moiActor != nullptr))
		{
			return false;
		}

		FQuat assemblyRot = FRotationMatrix::MakeFromYZ(AssemblyTangent, AssemblyNormal).ToQuat();
		OutRotation = moiActor->GetActorQuat() * assemblyRot * FQuat(FVector::UpVector, 1.5f * PI);

		return true;
	}

	bool FAdjustFFERotateHandle::HandleInputNumber(float number)
	{
		FEditModelAdjustmentHandleBase::OnEndUse();

		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

		float clockwiseScale = bClockwise ? 1.f : -1.f;
		float radians = FMath::DegreesToRadians(number);
		FQuat deltaRot(AssemblyNormal, radians * clockwiseScale);

		MOI->SetObjectRotation(OriginalRotation);

		TArray<int32> objectIDs = { MOI->ID };
		Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kRotateObjects)
			.Param(Parameters::kObjectIDs, objectIDs)
			.Param(Parameters::kOrigin, MOI->GetObjectLocation())
			.Param(Parameters::kQuaternion, deltaRot));

		MOI->ShowAdjustmentHandles(Controller.Get(), true);
		return true;
	}

	FQuat FAdjustFFERotateHandle::GetAnchorQuatFromCursor()
	{
		FVector basisV = AnchorDirLocation - AnchorLoc;
		basisV.Normalize();

		const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
		FVector projectedCursorPos = cursor.SketchPlaneProject(cursor.WorldPosition);
		FVector refV = (projectedCursorPos - AnchorLoc).GetSafeNormal();

		if (FVector::Coincident(basisV, refV))
		{
			return FQuat::Identity;
		}
		else if (FVector::Parallel(basisV, refV))
		{
			return FQuat(AssemblyNormal, PI);
		}
		else 
		{
			float unsignedAngle = FMath::Acos(basisV | refV);
			float angleSign = FMath::Sign((basisV ^ refV) | cursor.AffordanceFrame.Normal);
			return FQuat(AssemblyNormal, angleSign * unsignedAngle);
		}
	}

	bool FAdjustFFEInvertHandle::OnBeginUse()
	{
		UE_LOG(LogCallTrace, Display, TEXT("FAdjustFFEInvertHandle::OnBeginUse"));

		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		TArray<int32> ids = { MOI->ID };
		Controller->ModumateCommand(
			FModumateCommand(Commands::kInvertObjects)
			.Param(Parameters::kObjectIDs, ids));

		OnEndUse();
		return false;
	}

	FVector FAdjustFFEInvertHandle::GetAttachmentPoint()
	{
		FVector actorOrigin; FVector actorExtent;
		MOI->GetActor()->GetActorBounds(false, actorOrigin, actorExtent);
		return actorOrigin + FVector(0.f, 0.f, actorExtent.Z);
	}
}
