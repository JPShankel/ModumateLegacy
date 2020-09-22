#include "ToolsAndAdjustments/Handles/AdjustFFEPointHandle.h"

#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

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

	OriginalObjectLoc = TargetMOI->GetActor()->GetActorLocation();
	OriginalObjectRot = TargetMOI->GetObjectRotation();

	FVector curObjectNormal = OriginalObjectRot.RotateVector(AssemblyNormal);
	FVector curObjectTangent = OriginalObjectRot.RotateVector(AssemblyTangent);

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, curObjectNormal, curObjectTangent);

	return true;
}

bool AAdjustFFEPointHandle::UpdateUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustPolyPointHandle::UpdateUse"));

	Super::UpdateUse();

	FTransform newTransform;
	if (!GetTransform(newTransform))
	{
		return false;
	}

	TMap<int32, FTransform> objectInfo;
	objectInfo.Add(TargetMOI->ID, newTransform);
	FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, Controller->GetDocument(), Controller->GetWorld(), true);

	return true;
}

bool AAdjustFFEPointHandle::GetTransform(FTransform &OutTransform)
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

	OutTransform = moiActor->GetActorTransform();

	// If we have a hit normal, then compute a mounting transform based on the cursor.
	if (UModumateObjectStatics::GetFFEMountedTransform(&TargetMOI->GetAssembly(), cursor, OutTransform))
	{
		// If we hit a face that isn't coincident with the original mounting plane,
		// then use the new mounting transform's rotation, as if we're placing the FF&E for the first time.
		if (FVector::Coincident(cursor.HitNormal, cursor.AffordanceFrame.Normal))
		{
			OutTransform.SetRotation(OriginalObjectRot);
		}

		FVector newHandleOffset = OutTransform.TransformVector(LocalHandlePos);
		FVector newHandlePlanarOffset = FVector::VectorPlaneProject(newHandleOffset, cursor.HitNormal);
		FVector newActorPos = OutTransform.GetLocation() - newHandlePlanarOffset;

		OutTransform.SetLocation(newActorPos);
	}
	// Otherwise, just translate the FF&E along the sketch plane
	else
	{
		FVector translation = (cursor.SketchPlaneProject(cursor.WorldPosition) - AnchorLoc);
		OutTransform.SetLocation(OriginalObjectLoc + translation);
		OutTransform.SetRotation(OriginalObjectRot);
	}

	return true;
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

	FTransform newTransform;
	if (!GetTransform(newTransform))
	{
		return false;
	}

	FVector direction = newTransform.GetTranslation() - OriginalObjectLoc;
	direction.Normalize();

	FVector delta = number * direction;
	TMap<int32, FTransform> objectInfo;
	objectInfo.Add(TargetMOI->ID, FTransform(newTransform.GetRotation(), OriginalObjectLoc + delta));
	FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, Controller->GetDocument(), Controller->GetWorld(), false);

	if (!IsActorBeingDestroyed())
	{
		PostEndOrAbort();
	}

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
