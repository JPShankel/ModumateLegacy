// Copyright 2019 Modumate, Inc. All Rights Reserved.


#include "EditModelRotateTool.h"
#include "EditModelToolbase.h"
#include "EditModelSelectTool.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateCommands.h"
#include "LineActor.h"
#include "ModumateFunctionLibrary.h"

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
	return UEditModelToolBase::Deactivate();
}

bool URotateObjectTool::BeginUse()
{
	Super::BeginUse();

	AcquireSelectedObjects();

	PendingSegmentStart = Controller->GetWorld()->SpawnActor<ALineActor>();
	PendingSegmentEnd = Controller->GetWorld()->SpawnActor<ALineActor>();
	if (Controller->EMPlayerState->SnappedCursor.Visible)
	{
		Stage = AnchorPlaced;
		AnchorPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorPoint, FVector::UpVector);
		return true;
	}

	return false;
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
		RestoreSelectedObjects();

		// Should the new angle be calculate with input from mouse (CalcToolAngle) or from textinput
		float angle = bOverrideAngleWithInput ? InputAngle : CalcToolAngle();
		FRotator rot = FRotator(0, angle, 0);

		TArray<int32> ids;
		ids.Reset(OriginalObjectData.Num());
		for (auto &kvp : OriginalObjectData)
		{
			ids.Add(kvp.Key->ID);
		}
		ReleaseSelectedObjects();

		Controller->ModumateCommand(
			FModumateCommand(Commands::kRotateObjects)
			.Param(Parameters::kObjectIDs, ids)
			.Param(Parameters::kOrigin, AnchorPoint)
			.Param(Parameters::kQuaternion, rot.Quaternion()));


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

	return UEditModelToolBase::EndUse();
}

bool URotateObjectTool::AbortUse()
{
	RestoreSelectedObjects();
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

	for (auto &kvp : OriginalObjectData)
	{
		kvp.Key->SetFromDataRecordAndRotation(kvp.Value, AnchorPoint, rot);
	}
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
				CalcToolAngle(),
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

