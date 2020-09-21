#include "ToolsAndAdjustments/Handles/AdjustFFEPointHandle.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

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
	// Fail out if the actor is ever unavailable
	AActor *moiActor = TargetMOI->GetActor();
	if (moiActor == nullptr)
	{
		return false;
	}

	// Bail out of updating the target, but keep the handle active, if the cursor doesn't have a valid target
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	if (!cursor.Visible)
	{
		return true;
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

	FVector delta = number * moveAxis;
	TargetMOI->SetObjectLocation(OriginalObjectLoc + delta);

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
		TargetMOI->GetAssembly().TryGetProperty(BIMPropertyNames::Normal, AssemblyNormal) &&
		TargetMOI->GetAssembly().TryGetProperty(BIMPropertyNames::Tangent, AssemblyTangent);
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
