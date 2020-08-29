// Copyright 2019 Modumate, Inc. All Rights Reserved.


#include "ToolsAndAdjustments/Tools/EditModelRotateTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
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
		Stage = SelectingAngle;
		AngleAnchor = Controller->EMPlayerState->SnappedCursor.WorldPosition; //SetAnchorLocation
		return true;

	case SelectingAngle:
		return false;
	};
	return false;
}

bool URotateObjectTool::EndUse()
{
	if (IsInUse())
	{
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		ReleaseObjectsAndApplyDeltas();
	}

	if (PendingSegmentStart.IsValid())
	{
		PendingSegmentStart->Destroy();
	}

	if (PendingSegmentEnd.IsValid())
	{
		PendingSegmentEnd->Destroy();
	}

	return UEditModelToolBase::EndUse();
}

bool URotateObjectTool::AbortUse()
{
	ReleaseSelectedObjects();

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

	return UEditModelToolBase::AbortUse();
}

void URotateObjectTool::ApplyRotation()
{
	// TODO: rotation on affordance plane?
	float angle = CalcToolAngle();
	FQuat rot = FRotator(0, angle, 0).Quaternion();

	TMap<int32, FVector> objectInfo;
	for (auto& kvp : OriginalCornerTransforms)
	{
		objectInfo.Add(kvp.Key, AnchorPoint + rot.RotateVector(kvp.Value.GetTranslation() - AnchorPoint));
	}

	FModumateObjectDeltaStatics::PreviewMovement(objectInfo, Controller->GetDocument(), Controller->GetWorld());
	
	// TODO: find a replacement for control points to account for non graph-hosted objects in PreviewMovement

	//for (auto &kvp : OriginalCornerTransforms)
	//{
	//	kvp.Key->SetFromDataRecordAndRotation(kvp.Value, AnchorPoint, rot);
	//}
}

float URotateObjectTool::CalcToolAngle()
{
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return 0;
	}

	FVector basisV = AngleAnchor - AnchorPoint;
	basisV.Normalize();
	FVector refV = Controller->EMPlayerState->SnappedCursor.WorldPosition - AnchorPoint;
	refV.Normalize();

	FVector cr = FVector::CrossProduct(basisV, refV);
	float dp = FGenericPlatformMath::Acos(FVector::DotProduct(basisV, refV));

	if (cr.Z < 0)
	{
		dp = -dp;
	}
	return dp * 180.0f / PI;
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
			PendingSegmentStart->Color = FColor::White;
			PendingSegmentStart->Thickness = 1;
			// Draw a single line with tick mark for rotate tool
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				AnchorPoint,
				hitLoc,
				FVector::ZeroVector,
				Controller->DimensionStringGroupID_PlayerController,
				Controller->DimensionStringUniqueID_Delta,
				0,
				Controller,
				EDimStringStyle::RotateToolLine,
				EEnterableField::None,
				40.f,
				EAutoEditableBox::Never,
				true,
				FColor::White);
			// Drawing protractor
			Controller->DrawRotateToolDegree(CalcToolAngle(), AnchorPoint, PendingSegmentStart.Get(), nullptr);
		}
		if (Stage == SelectingAngle)
		{
			PendingSegmentStart->Point1 = AnchorPoint;
			PendingSegmentStart->Point2 = hitLoc;
			PendingSegmentStart->Color = FColor::White;
			PendingSegmentStart->Thickness = 1;
			// Add line between anchor point and cursor pos - Delta Only
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				AnchorPoint,
				hitLoc,
				FVector::ZeroVector,
				Controller->DimensionStringGroupID_PlayerController,
				Controller->DimensionStringUniqueID_Delta,
				0,
				Controller,
				EDimStringStyle::RotateToolLine,
				EEnterableField::None,
				40.f,
				EAutoEditableBox::Never,
				true,
				FColor::White);

			PendingSegmentEnd->Point1 = AnchorPoint;
			PendingSegmentEnd->Point2 = AngleAnchor;
			PendingSegmentEnd->Color = FColor::Green;
			PendingSegmentEnd->Thickness = 1;
			// Add line between anchor point and location of angle anchor - Delta only
			UModumateFunctionLibrary::AddNewDimensionString(
				Controller,
				AnchorPoint,
				AngleAnchor,
				FVector::ZeroVector,
				Controller->DimensionStringGroupID_PlayerController,
				Controller->DimensionStringUniqueID_Delta,
				0,
				Controller,
				EDimStringStyle::RotateToolLine,
				EEnterableField::None,
				40.f,
				EAutoEditableBox::Never,
				true,
				FColor::Green);
			UModumateFunctionLibrary::AddNewDegreeString(
				Controller,
				AnchorPoint, // location
				hitLoc, // start
				AngleAnchor, // end
				false, // TODO: this was CalcToolAngle(), which was clearly wrong, but needs to be refactored with new dimension strings soon
				FName(TEXT("RotateTool")),
				FName(TEXT("Degree")),
				0,
				Controller,
				EDimStringStyle::DegreeString,
				EEnterableField::NonEditableText,
				EAutoEditableBox::UponUserInput,
				true);

			ApplyRotation();

			// Drawing protractor
			Controller->DrawRotateToolDegree(CalcToolAngle(), AnchorPoint, PendingSegmentStart.Get(), PendingSegmentEnd.Get());
		}
	}
	return true;
}

bool URotateObjectTool::HandleInputNumber(double n)
{
	bOverrideAngleWithInput = true; // Tell enduse to use angle from text input instead of calculate from cursor
	InputAngle = float(n);
	bOverrideAngleWithInput = false; // After commit with the new rotation, no need to override the angle anymore
	return EndUse();
}

