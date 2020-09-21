#include "ToolsAndAdjustments/Handles/AdjustFFERotateHandle.h"

#include "Algo/Accumulate.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

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
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	float clockwiseScale = bClockwise ? 1.f : -1.f;
	float radians = FMath::DegreesToRadians(number);
	FQuat deltaRot(AssemblyNormal, radians * clockwiseScale);

	TargetMOI->SetObjectRotation(deltaRot * OriginalRotation);
	EndUse();

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
		TargetMOI->GetAssembly().TryGetProperty(BIMPropertyNames::Normal, AssemblyNormal) &&
		TargetMOI->GetAssembly().TryGetProperty(BIMPropertyNames::Tangent, AssemblyTangent);
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
