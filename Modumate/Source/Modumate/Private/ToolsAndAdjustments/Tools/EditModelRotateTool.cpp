// Copyright 2019 Modumate, Inc. All Rights Reserved.


#include "EditModelRotateTool.h"
#include "EditModelToolbase.h"
#include "EditModelSelectTool.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateCommands.h"
#include "LineActor3D_CPP.h"
#include "ModumateFunctionLibrary.h"

namespace Modumate
{
	FRotateObjectTool::FRotateObjectTool(AEditModelPlayerController_CPP *pc)
		: FEditModelToolBase(pc)
		, FSelectedObjectToolMixin(pc)
		, AnchorPoint(ForceInitToZero)
		, AngleAnchor(ForceInitToZero)
		, Stage(Neutral)
	{ }

	FRotateObjectTool::~FRotateObjectTool() {}

	bool FRotateObjectTool::Activate()
	{
		FEditModelToolBase::Activate();
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

		Stage = Neutral;
		return true;
	}

	bool FRotateObjectTool::Deactivate()
	{
		if (IsInUse())
		{
			AbortUse();
		}
		return FEditModelToolBase::Deactivate();
	}

	bool FRotateObjectTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();

		AcquireSelectedObjects();

		PendingSegmentStart = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(AEditModelGameMode_CPP::LineClass);
		PendingSegmentEnd = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(AEditModelGameMode_CPP::LineClass);
		if (Controller->EMPlayerState->SnappedCursor.Visible)
		{
			Stage = AnchorPlaced;
			AnchorPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorPoint, FVector::UpVector);
			return true;
		}

		return false;
	}

	bool FRotateObjectTool::EnterNextStage()
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

	bool FRotateObjectTool::EndUse()
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

		return FEditModelToolBase::EndUse();
	}

	bool FRotateObjectTool::AbortUse()
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

		return FEditModelToolBase::AbortUse();
	}

	void FRotateObjectTool::ApplyRotation()
	{
		// TODO: rotation on affordance plane?
		float angle = CalcToolAngle();
		FQuat rot = FRotator(0, angle, 0).Quaternion();

		for (auto &kvp : OriginalObjectData)
		{
			kvp.Key->SetFromDataRecordAndRotation(kvp.Value, AnchorPoint, rot);
		}
	}

	float FRotateObjectTool::CalcToolAngle()
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

	bool FRotateObjectTool::FrameUpdate()
	{
		FEditModelToolBase::FrameUpdate();
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
				PendingSegmentStart->AntiAliasing = true;
				PendingSegmentStart->ScreenSize = false;
				// Draw a single line with tick mark for rotate tool
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller.Get(),
					AnchorPoint,
					hitLoc,
					FVector::ZeroVector,
					Controller->DimensionStringGroupID_PlayerController,
					Controller->DimensionStringUniqueID_Delta,
					0,
					Controller.Get(),
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
				PendingSegmentStart->AntiAliasing = true;
				PendingSegmentEnd->ScreenSize = false;
				// Add line between anchor point and cursor pos - Delta Only
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller.Get(),
					AnchorPoint,
					hitLoc,
					FVector::ZeroVector,
					Controller->DimensionStringGroupID_PlayerController,
					Controller->DimensionStringUniqueID_Delta,
					0,
					Controller.Get(),
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
				PendingSegmentEnd->AntiAliasing = true;
				PendingSegmentEnd->IsAnchor = true;
				PendingSegmentEnd->ScreenSize = true;
				// Add line between anchor point and location of angle anchor - Delta only
				UModumateFunctionLibrary::AddNewDimensionString(
					Controller.Get(),
					AnchorPoint,
					AngleAnchor,
					FVector::ZeroVector,
					Controller->DimensionStringGroupID_PlayerController,
					Controller->DimensionStringUniqueID_Delta,
					0,
					Controller.Get(),
					EDimStringStyle::RotateToolLine,
					EEnterableField::None,
					40.f,
					EAutoEditableBox::Never,
					true,
					FColor::Green);
				UModumateFunctionLibrary::AddNewDegreeString(
					Controller.Get(),
					AnchorPoint, // location
					hitLoc, // start
					AngleAnchor, // end
					CalcToolAngle(),
					FName(TEXT("RotateTool")),
					FName(TEXT("Degree")),
					0,
					Controller.Get(),
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

	bool FRotateObjectTool::HandleInputNumber(double n)
	{
		bOverrideAngleWithInput = true; // Tell enduse to use angle from text input instead of calculate from cursor
		InputAngle = float(n);
		bOverrideAngleWithInput = false; // After commit with the new rotation, no need to override the angle anymore
		return EndUse();
	}

	IModumateEditorTool *MakeRotateObjectTool(AEditModelPlayerController_CPP *controller)
	{
		return new FRotateObjectTool(controller);
	}
}