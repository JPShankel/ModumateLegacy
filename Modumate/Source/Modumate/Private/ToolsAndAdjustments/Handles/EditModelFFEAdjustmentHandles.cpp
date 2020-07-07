// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Handles/EditModelFFEAdjustmentHandles.h"
#include "Algo/Accumulate.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "DocumentManagement/ModumateCommands.h"

using namespace Modumate;

AAdjustFFEPointHandle::AAdjustFFEPointHandle(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
	, AssemblyNormal(FVector::UpVector)
	, AssemblyTangent(FVector::RightVector)
	, LocalHandlePos(ForceInitToZero)
{ }

bool AAdjustFFEPointHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustPolyPointHandle::BeginUse"));

	if (!Super::BeginUse())
	{
		return false;
	}

	FVector actorLoc = GetHandlePosition();
	AnchorLoc = FVector(actorLoc.X, actorLoc.Y, TargetMOI->GetActor()->GetActorLocation().Z);

	OriginalHandleLoc = GetHandlePosition();
	OriginalObjectLoc = TargetMOI->GetActor()->GetActorLocation();
	OriginalObjectRot = TargetMOI->GetObjectRotation();

	FVector curObjectNormal = OriginalObjectRot.RotateVector(AssemblyNormal);
	FVector curObjectTangent = OriginalObjectRot.RotateVector(AssemblyTangent);

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(OriginalHandleLoc, curObjectNormal, curObjectTangent);

	return true;
}

bool AAdjustFFEPointHandle::UpdateUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustPolyPointHandle::UpdateUse"));

	Super::UpdateUse();

	if (!UpdateTarget())
	{
		return false;
	}

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	FTransform newTransform = TargetMOI->GetActor()->GetActorTransform();
	FVector translation = newTransform.GetLocation() - OriginalObjectLoc;

	// If the object has moved along its existing mounting plane, then show dimension strings
	if (newTransform.GetRotation().Equals(OriginalObjectRot) &&
		FMath::IsNearlyZero(translation | cursor.AffordanceFrame.Normal, PLANAR_DOT_EPSILON))
	{
		FVector newHandleLoc = GetHandlePosition();

		FVector axisTangent = cursor.AffordanceFrame.Tangent;
		FVector axisBitangent = cursor.AffordanceFrame.Tangent ^ cursor.AffordanceFrame.Normal;

		FVector offsetTangent = translation.ProjectOnToNormal(axisTangent);
		FVector offsetBitangent = translation.ProjectOnToNormal(axisBitangent);

		// Add tangent dimension string (repurposing the Delta ID)
		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			newHandleLoc - offsetTangent,
			newHandleLoc,
			axisBitangent,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Delta,
			0,
			TargetMOI->GetActor(),
			EDimStringStyle::HCamera);

		// Add bitangent dimension string (repurposing the Total ID)
		UModumateFunctionLibrary::AddNewDimensionString(
			Controller,
			newHandleLoc - offsetBitangent,
			newHandleLoc,
			axisTangent,
			Controller->DimensionStringGroupID_Default,
			Controller->DimensionStringUniqueID_Total,
			1,
			TargetMOI->GetActor(),
			EDimStringStyle::Fixed);
	}

	return true;
}

bool AAdjustFFEPointHandle::UpdateTarget()
{
	AActor *moiActor = TargetMOI->GetActor();
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
	if (UModumateObjectStatics::GetFFEMountedTransform(&TargetMOI->GetAssembly(), cursor, newTransform))
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

	TargetMOI->SetWorldTransform(newTransform);
	//moiActor->SetActorTransform(newTransform);

	return true;
}

void AAdjustFFEPointHandle::EndUse()
{
	// Update the target now, in case the cursor has changed since the last UpdateUse (i.e. during HandleInputNumber)
	UpdateTarget();

	Super::EndUse();
}

void AAdjustFFEPointHandle::AbortUse()
{
	Super::AbortUse();

	AActor *moiActor = TargetMOI ? TargetMOI->GetActor() : nullptr;
	if (moiActor)
	{
		moiActor->SetActorTransform(FTransform(OriginalObjectRot, OriginalObjectLoc));
	}
}

FVector AAdjustFFEPointHandle::GetHandlePosition() const
{
	AActor *moiActor = TargetMOI->GetActor();
	if (!ensure(moiActor != nullptr))
	{
		return FVector::ZeroVector;
	}

	return moiActor->GetActorTransform().TransformPosition(LocalHandlePos);
}

bool AAdjustFFEPointHandle::HandleInputNumber(float number)
{
	AActor *moiActor = TargetMOI ? TargetMOI->GetActor() : nullptr;
	if (!(moiActor && Controller))
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

	TArray<int32> objectIDs = { TargetMOI->ID };
	FVector delta = number * moveAxis;

	moiActor->SetActorLocation(OriginalObjectLoc);
	Controller->ModumateCommand(
		FModumateCommand(Modumate::Commands::kMoveObjects)
		.Param(Parameters::kObjectIDs, objectIDs)
		.Param(Parameters::kDelta, delta));

	EndUse();
	return true;
}

void AAdjustFFEPointHandle::Initialize()
{
	Super::Initialize();

	if (!ensure(TargetMOI))
	{
		return;
	}

	bool bHaveMountingProperties =
		TargetMOI->GetAssembly().TryGetProperty(BIM::Parameters::Normal, AssemblyNormal) &&
		TargetMOI->GetAssembly().TryGetProperty(BIM::Parameters::Tangent, AssemblyTangent);
	ensure(bHaveMountingProperties);

	AActor *moiActor = TargetMOI->GetActor();

	TArray<FVector> boxSidePoints;
	if (ensure(moiActor && (TargetIndex >= 0) && (TargetIndex < 4)) &&
		UModumateObjectStatics::GetFFEBoxSidePoints(moiActor, AssemblyNormal, boxSidePoints))
	{
		FVector actorLoc = moiActor->GetActorLocation();
		FQuat actorRot = moiActor->GetActorQuat();
		FVector curObjectNormal = actorRot.RotateVector(AssemblyNormal);
		FVector handleCornerPos = boxSidePoints[TargetIndex];
		FVector curHandlePos = FVector::PointPlaneProject(handleCornerPos, actorLoc, curObjectNormal);
		LocalHandlePos = moiActor->GetActorTransform().InverseTransformPosition(curHandlePos);
	}
}

bool AAdjustFFEPointHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->GenericPointStyle;
	OutWidgetSize = FVector2D(12.0f, 12.0f);
	return true;
}


bool AAdjustFFERotateHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustFFERotateHandle::BeginUse"));

	if (!Super::BeginUse())
	{
		return false;
	}
	AnchorLoc = TargetMOI->GetObjectLocation();
	OriginalRotation = TargetMOI->GetObjectRotation();

	FVector curObjectNormal = OriginalRotation.RotateVector(AssemblyNormal);
	FVector curObjectTangent = OriginalRotation.RotateVector(AssemblyTangent);

	// Choose one of the two for defining AnchorDirLocation
	//AnchorDirection = GetPlaneCursorHit(AnchorLoc, mousex, mousey); // #1: Use mouse pos to set AnchorDirLocation
	AnchorDirLocation = AnchorLoc + (curObjectTangent * 200.f); // #2: Use object rotation to set AnchorDirLocation

	// Setup location for world axis snap
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, curObjectNormal, curObjectTangent);

	return true;
}

bool AAdjustFFERotateHandle::UpdateUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustFFERotateHandle::UpdateUse"));

	Super::UpdateUse();

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	FVector projectedCursorPos = cursor.SketchPlaneProject(cursor.WorldPosition);

	Super::UpdateUse();
	if (Controller->EMPlayerState->SnappedCursor.Visible)
	{
		FQuat originalQuat = OriginalRotation;
		FQuat userQuat = GetAnchorQuatFromCursor();
		FVector userAxis;
		float userAngle;
		userQuat.ToAxisAndAngle(userAxis, userAngle);
		bClockwise = ((userAxis | cursor.AffordanceFrame.Normal) > 0.0f);
		FQuat quatRot = originalQuat * userQuat;

		TargetMOI->SetObjectRotation(quatRot);

		// Start to draw dim string when object has been rotated
		if (!TargetMOI->GetObjectRotation().Equals(OriginalRotation, 0.1f))
		{
			UModumateFunctionLibrary::AddNewDegreeString(
				Controller,
				AnchorLoc, //degree location
				AnchorDirLocation, // start
				projectedCursorPos, // end
				bClockwise,
				Controller->DimensionStringGroupID_PlayerController, // ControllerGroup
				Controller->DimensionStringUniqueID_Delta,
				0,
				Controller,
				EDimStringStyle::DegreeString,
				EEnterableField::NonEditableText,
				EAutoEditableBox::UponUserInput,
				true);

			// Line dim strings needs to be separate group from degree string to prevent TAB switching
			FName rotateDimLineGroup = FName(TEXT("DimLine"));

			// Dim string from AnchorLoc to cursor
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				AnchorLoc,
				projectedCursorPos,
				FVector::ZeroVector,
				rotateDimLineGroup,
				Controller->DimensionStringUniqueID_Delta,
				1,
				Controller,
				EDimStringStyle::RotateToolLine,
				EEnterableField::None,
				40.f,
				EAutoEditableBox::Never,
				true,
				FColor::White);

			// Dim string from AnchorLoc to AnchorAngle
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				AnchorLoc,
				AnchorDirLocation,
				FVector::ZeroVector,
				rotateDimLineGroup,
				Controller->DimensionStringUniqueID_Delta,
				1,
				Controller,
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

FVector AAdjustFFERotateHandle::GetHandlePosition() const
{
	AActor *moiActor = TargetMOI->GetActor();
	if (!ensure(moiActor != nullptr))
	{
		return FVector::ZeroVector;
	}

	return moiActor->GetActorTransform().TransformPosition(LocalHandlePos);
}

FVector AAdjustFFERotateHandle::GetHandleDirection() const
{
	AActor *moiActor = TargetMOI ? TargetMOI->GetActor() : nullptr;
	if (!ensure(moiActor))
	{
		return FVector::ZeroVector;
	}

	return moiActor->GetActorQuat().RotateVector(AssemblyNormal);
}

bool AAdjustFFERotateHandle::HandleInputNumber(float number)
{
	Super::EndUse();

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	float clockwiseScale = bClockwise ? 1.f : -1.f;
	float radians = FMath::DegreesToRadians(number);
	FQuat deltaRot(AssemblyNormal, radians * clockwiseScale);

	TargetMOI->SetObjectRotation(OriginalRotation);

	TArray<int32> objectIDs = { TargetMOI->ID };
	Controller->ModumateCommand(
		FModumateCommand(Modumate::Commands::kRotateObjects)
		.Param(Parameters::kObjectIDs, objectIDs)
		.Param(Parameters::kOrigin, TargetMOI->GetObjectLocation())
		.Param(Parameters::kQuaternion, deltaRot));

	return true;
}

FQuat AAdjustFFERotateHandle::GetAnchorQuatFromCursor()
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

void AAdjustFFERotateHandle::Initialize()
{
	Super::Initialize();

	if (!ensure(TargetMOI))
	{
		return;
	}

	bool bHaveMountingProperties =
		TargetMOI->GetAssembly().TryGetProperty(BIM::Parameters::Normal, AssemblyNormal) &&
		TargetMOI->GetAssembly().TryGetProperty(BIM::Parameters::Tangent, AssemblyTangent);
	ensure(bHaveMountingProperties);

	AActor *moiActor = TargetMOI->GetActor();

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

bool AAdjustFFERotateHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->RotateStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);
	return true;
}


bool AAdjustFFEInvertHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustFFEInvertHandle::BeginUse"));

	if (!Super::BeginUse())
	{
		return false;
	}

	TargetMOI->InvertObject();

	EndUse();
	return false;
}

FVector AAdjustFFEInvertHandle::GetHandlePosition() const
{
	FVector actorOrigin; FVector actorExtent;
	TargetMOI->GetActor()->GetActorBounds(false, actorOrigin, actorExtent);
	return actorOrigin + FVector(0.f, 0.f, actorExtent.Z);
}

FVector AAdjustFFEInvertHandle::GetHandleDirection() const
{
	// Return the world axis along which the FF&E would be inverted
	return TargetMOI->GetActor()->GetActorQuat().RotateVector(FVector(0.0f, 1.0f, 0.0f));
}

bool AAdjustFFEInvertHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->InvertStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);
	return true;
}
