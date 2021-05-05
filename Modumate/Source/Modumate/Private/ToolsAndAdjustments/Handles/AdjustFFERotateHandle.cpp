#include "ToolsAndAdjustments/Handles/AdjustFFERotateHandle.h"

#include "Algo/Accumulate.h"
#include "Components/EditableTextBox.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UI/AdjustmentHandleAssetData.h"
#include "UI/DimensionManager.h"
#include "UI/PendingAngleActor.h"
#include "UI/EditModelPlayerHUD.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

AAdjustFFERotateHandle::AAdjustFFERotateHandle()
	: Super()
	, AssemblyNormal(FVector::UpVector)
	, AssemblyTangent(FVector::RightVector)
{ }

bool AAdjustFFERotateHandle::BeginUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustFFERotateHandle::BeginUse"));

	if (!Super::BeginUse())
	{
		return false;
	}
	AnchorLoc = TargetMOI->GetLocation();
	OriginalRotation = TargetMOI->GetRotation();

	FVector curObjectNormal = OriginalRotation.RotateVector(AssemblyNormal);
	FVector curObjectTangent = OriginalRotation.RotateVector(AssemblyTangent);

	// Choose one of the two for defining AnchorDirLocation
	//AnchorDirection = GetPlaneCursorHit(AnchorLoc, mousex, mousey); // #1: Use mouse pos to set AnchorDirLocation
	AnchorDirLocation = AnchorLoc + (curObjectTangent * 200.f); // #2: Use object rotation to set AnchorDirLocation

	// Setup location for world axis snap
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorLoc, curObjectNormal, curObjectTangent);

	BaseLine->SetVisibilityInApp(true);
	RotationLine->SetVisibilityInApp(true);

	BaseLine->Point2 = AnchorDirLocation;

	APendingAngleActor* dimensionActor = Cast<APendingAngleActor>(GameInstance->DimensionManager->AddDimensionActor(APendingAngleActor::StaticClass()));
	dimensionActor->SetHidden(false);
	PendingSegmentID = dimensionActor->ID;
	dimensionActor->WorldPosition = TargetMOI->GetLocation();
	dimensionActor->WorldDirection = BaseLine->Point2 - BaseLine->Point1;
	dimensionActor->WorldDirection.Normalize();

	auto dimensionWidget = dimensionActor->DimensionText;
	dimensionWidget->Measurement->SetIsReadOnly(false);
	dimensionWidget->Measurement->OnTextCommitted.AddDynamic(this, &AAdjustmentHandleActor::OnTextCommitted);

	GameInstance->DimensionManager->SetActiveActorID(PendingSegmentID);

	return true;
}

bool AAdjustFFERotateHandle::UpdateUse()
{
	UE_LOG(LogCallTrace, Display, TEXT("AAdjustFFERotateHandle::UpdateUse"));

	Super::UpdateUse();

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	FVector projectedCursorPos = cursor.SketchPlaneProject(cursor.WorldPosition);

	BaseLine->Point2 = AnchorDirLocation;
	RotationLine->Point2 = projectedCursorPos;

	FTransform newTransform;
	if (!GetTransform(newTransform))
	{
		return false;
	}

	TMap<int32, FTransform> objectInfo;
	objectInfo.Add(TargetMOI->ID, newTransform);
	FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, Controller->GetDocument(), Controller->GetWorld(), true);


	FVector baseDir = BaseLine->Point2 - BaseLine->Point1;
	FVector rotationDir = RotationLine->Point2 - RotationLine->Point1;


	return true;
}

void AAdjustFFERotateHandle::PostEndOrAbort()
{
	BaseLine->SetVisibilityInApp(false);
	RotationLine->SetVisibilityInApp(false);

	Super::PostEndOrAbort();
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

	TMap<int32, FTransform> objectInfo;
	objectInfo.Add(TargetMOI->ID, FTransform(deltaRot * OriginalRotation, TargetMOI->GetLocation()));
	FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, Controller->GetDocument(), Controller->GetWorld(), false);

	if (!IsActorBeingDestroyed())
	{
		PostEndOrAbort();
	}

	return true;
}

FQuat AAdjustFFERotateHandle::GetAnchorQuatFromCursor()
{
	// TODO: this function should probably be static and replace RotateTool::CalcToolAngle
	FVector basisV = OriginalRotation.RotateVector(AssemblyTangent);

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	FVector projectedCursorPos = cursor.SketchPlaneProject(cursor.WorldPosition);
	FVector refV = (projectedCursorPos - AnchorLoc).GetSafeNormal();
	refV.Normalize();

	FVector cr = FVector::CrossProduct(basisV, refV);
	float dp = FMath::Acos(FVector::DotProduct(basisV, refV));

	if (cr.Z < 0)
	{
		dp = -dp;
	}

	auto dimensionActor = Cast<APendingAngleActor>(GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID));

	FVector offsetDirection = (refV + basisV) / 2.0f;
	offsetDirection.Normalize();

	dimensionActor->WorldDirection = offsetDirection * -1.0f;

	return FQuat(AssemblyNormal, dp);
}

bool AAdjustFFERotateHandle::HasDimensionActor()
{
	return false;
}

bool AAdjustFFERotateHandle::HasDistanceTextInput()
{
	return false;
}

void AAdjustFFERotateHandle::Initialize()
{
	Super::Initialize();

	if (!ensure(TargetMOI))
	{
		return;
	}

	AActor *moiActor = TargetMOI->GetActor();

	TArray<FVector> boxSidePoints;
	if (ensure(moiActor) &&
		UModumateObjectStatics::GetFFEBoxSidePoints(moiActor, TargetMOI->GetAssembly().Normal, boxSidePoints))
	{
		FVector actorLoc = moiActor->GetActorLocation();
		FQuat actorRot = moiActor->GetActorQuat();
		FVector curObjectNormal = actorRot.RotateVector(TargetMOI->GetAssembly().Normal);
		FVector averageBoxSidePos = Algo::Accumulate(boxSidePoints, FVector::ZeroVector) / boxSidePoints.Num();
		FVector curHandlePos = FVector::PointPlaneProject(averageBoxSidePos, actorLoc, curObjectNormal);
		LocalHandlePos = moiActor->GetActorTransform().InverseTransformPosition(curHandlePos);
	}

	BaseLine = Controller->GetWorld()->SpawnActor<ALineActor>();
	RotationLine = Controller->GetWorld()->SpawnActor<ALineActor>();

	for (auto line : { BaseLine, RotationLine })
	{
		line->Point1 = TargetMOI->GetLocation();
		line->Color = SegmentColor;
		line->Thickness = SegmentThickness;
		line->SetVisibilityInApp(false);
	}
}

bool AAdjustFFERotateHandle::GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const
{
	OutButtonStyle = PlayerHUD->HandleAssets->RotateStyle;
	OutWidgetSize = FVector2D(48.0f, 48.0f);
	return true;
}

bool AAdjustFFERotateHandle::GetTransform(FTransform& OutTransform)
{
	// Fail out if the actor is ever unavailable
	AActor *moiActor = TargetMOI->GetActor();
	if (moiActor == nullptr)
	{
		return false;
	}

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	FQuat userQuat = GetAnchorQuatFromCursor();

	FVector userAxis;
	float userAngle;
	userQuat.ToAxisAndAngle(userAxis, userAngle);

	bClockwise = ((userAxis | cursor.AffordanceFrame.Normal) > 0.0f);
	FQuat quatRot = OriginalRotation * userQuat;

	auto dimensionActor = Cast<APendingAngleActor>(GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID));
	dimensionActor->CurrentAngle = userAngle;

	OutTransform = moiActor->GetActorTransform();
	OutTransform.SetRotation(quatRot);

	return true;
}
