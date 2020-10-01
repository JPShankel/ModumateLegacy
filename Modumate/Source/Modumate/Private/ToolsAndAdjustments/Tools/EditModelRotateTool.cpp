// Copyright 2019 Modumate, Inc. All Rights Reserved.


#include "ToolsAndAdjustments/Tools/EditModelRotateTool.h"

#include "Components/EditableTextBox.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UI/DimensionManager.h"
#include "UI/PendingAngleActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/LineActor.h"

using namespace Modumate;

URotateObjectTool::URotateObjectTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FSelectedObjectToolMixin(Controller)
	, AnchorPoint(ForceInitToZero)
	, AngleAnchor(ForceInitToZero)
	, Stage(Neutral)
{ }

bool URotateObjectTool::Activate()
{
	Super::Activate();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	Stage = Neutral;
	return true;
}

bool URotateObjectTool::Deactivate()
{
	if (IsInUse())
	{
		AbortUse();
	}

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return UEditModelToolBase::Deactivate();
}

bool URotateObjectTool::BeginUse()
{
	Super::BeginUse();

	AcquireSelectedObjects();

	PendingSegmentStart = Controller->GetWorld()->SpawnActor<ALineActor>();
	PendingSegmentEnd = Controller->GetWorld()->SpawnActor<ALineActor>();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	Stage = AnchorPlaced;
	AnchorPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorPoint, FVector::UpVector);
	bOriginalVerticalAffordanceSnap = Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return true;
}

bool URotateObjectTool::EnterNextStage()
{
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	switch (Stage)
	{
	case Neutral:
		return false;

	case AnchorPlaced:
	{
		Stage = SelectingAngle;
		AngleAnchor = Controller->EMPlayerState->SnappedCursor.WorldPosition; //SetAnchorLocation

		APendingAngleActor* dimensionActor = Cast<APendingAngleActor>(GameInstance->DimensionManager->AddDimensionActor(APendingAngleActor::StaticClass()));
		dimensionActor->SetHidden(false);
		PendingSegmentID = dimensionActor->ID;
		dimensionActor->WorldPosition = AnchorPoint;
		dimensionActor->WorldDirection = AngleAnchor - AnchorPoint;
		dimensionActor->WorldDirection.Normalize();

		auto dimensionWidget = dimensionActor->DimensionText;
		dimensionWidget->Measurement->SetIsReadOnly(false);
		dimensionWidget->Measurement->OnTextCommitted.AddDynamic(this, &UEditModelToolBase::OnTextCommitted);

		DimensionManager->SetActiveActorID(PendingSegmentID);

		return true;
	}

	case SelectingAngle:
		return false;
	};
	return false;
}

bool URotateObjectTool::EndUse()
{
	if (IsInUse())
	{
		ReleaseObjectsAndApplyDeltas();
	}

	return UEditModelToolBase::EndUse();
}

bool URotateObjectTool::AbortUse()
{
	ReleaseSelectedObjects();

	return UEditModelToolBase::AbortUse();
}

bool URotateObjectTool::PostEndOrAbort()
{
	if (IsInUse())
	{
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	}

	if (PendingSegmentStart.IsValid())
	{
		PendingSegmentStart->Destroy();
	}
	if (PendingSegmentEnd.IsValid())
	{
		PendingSegmentEnd->Destroy();
	}

	return Super::PostEndOrAbort();
}

void URotateObjectTool::ApplyRotation()
{
	// TODO: rotation on affordance plane?
	FQuat userQuat = CalcToolAngle();

	FVector userAxis;
	float userAngle;
	userQuat.ToAxisAndAngle(userAxis, userAngle);

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	bClockwise = ((userAxis | cursor.AffordanceFrame.Normal) > 0.0f);

	auto dimensionActor = Cast<APendingAngleActor>(GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID));
	dimensionActor->CurrentAngle = userAngle;

	TMap<int32, FTransform> objectInfo;
	for (auto& kvp : OriginalTransforms)
	{
		objectInfo.Add(kvp.Key, FTransform(userQuat * kvp.Value.GetRotation(), AnchorPoint + userQuat.RotateVector(kvp.Value.GetTranslation() - AnchorPoint)));
	}

	FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, Controller->GetDocument(), Controller->GetWorld(), true);
}

FQuat URotateObjectTool::CalcToolAngle()
{
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return FQuat::Identity;
	}

	FVector basisV = AngleAnchor - AnchorPoint;
	basisV.Normalize();
	FVector refV = Controller->EMPlayerState->SnappedCursor.WorldPosition - AnchorPoint;
	refV.Normalize();

	auto dimensionActor = Cast<APendingAngleActor>(GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID));

	FVector offsetDirection = (refV + basisV) / 2.0f;
	offsetDirection.Normalize();

	dimensionActor->WorldDirection = offsetDirection * -1.0f;

	FVector cr = FVector::CrossProduct(basisV, refV);
	float dp = FGenericPlatformMath::Acos(FVector::DotProduct(basisV, refV));

	if (cr.Z < 0)
	{
		dp = -dp;
	}

	return FQuat(FVector::UpVector, dp);
}

bool URotateObjectTool::FrameUpdate()
{
	Super::FrameUpdate();
	if (IsInUse() && Controller->EMPlayerState->SnappedCursor.Visible)
	{
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		hitLoc = FVector(hitLoc.X, hitLoc.Y, AnchorPoint.Z);
		if (Stage == AnchorPlaced)
		{
			PendingSegmentStart->Point1 = AnchorPoint;
			PendingSegmentStart->Point2 = hitLoc;
			PendingSegmentStart->Color = FColor::Black;
			PendingSegmentStart->Thickness = 3;
		}
		if (Stage == SelectingAngle)
		{
			PendingSegmentStart->Point1 = AnchorPoint;
			PendingSegmentStart->Point2 = hitLoc;
			PendingSegmentStart->Color = FColor::Black;
			PendingSegmentStart->Thickness = 3;

			PendingSegmentEnd->Point1 = AnchorPoint;
			PendingSegmentEnd->Point2 = AngleAnchor;
			PendingSegmentEnd->Color = FColor::Black;
			PendingSegmentEnd->Thickness = 3;

			ApplyRotation();
		}
	}
	return true;
}

bool URotateObjectTool::HandleInputNumber(double n)
{
	float clockwiseScale = bClockwise ? 1.f : -1.f;
	float radians = FMath::DegreesToRadians(static_cast<float>(n));
	FQuat deltaRot(FVector::UpVector, radians * clockwiseScale);

	TMap<int32, FTransform> objectInfo;
	for (auto& kvp : OriginalTransforms)
	{
		objectInfo.Add(kvp.Key, FTransform(deltaRot * kvp.Value.GetRotation(), AnchorPoint + deltaRot.RotateVector(kvp.Value.GetTranslation() - AnchorPoint)));
	}
	FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, Controller->GetDocument(), Controller->GetWorld(), false);

	ReleaseSelectedObjects();

	return PostEndOrAbort();
}

