#include "EditModelMoveTool.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameState_CPP.h"
#include "ModumateCommands.h"
#include "EditModelGameMode_CPP.h"
#include "LineActor.h"

using namespace Modumate;

UMoveObjectTool::UMoveObjectTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FSelectedObjectToolMixin(Controller)
{}

bool UMoveObjectTool::Activate()
{
	Super::Activate();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	return true;
}

bool UMoveObjectTool::Deactivate()
{
	if (IsInUse())
	{
		AbortUse();
	}
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	return UEditModelToolBase::Deactivate();
}

bool UMoveObjectTool::BeginUse()
{
	Super::BeginUse();

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	AcquireSelectedObjects();

	if (Controller->EMPlayerState->SnappedCursor.Visible)
	{
		AnchorPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorPoint, Controller->EMPlayerState->SnappedCursor.HitNormal, Controller->EMPlayerState->SnappedCursor.HitTangent);
		PendingMoveLine = Controller->GetWorld()->SpawnActor<ALineActor>();
		return true;
	}
	return false;
}

bool UMoveObjectTool::FrameUpdate()
{
	Super::FrameUpdate();

	if (IsInUse() && Controller->EMPlayerState->SnappedCursor.Visible)
	{
		const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

		if (PendingMoveLine.IsValid())
		{
			PendingMoveLine->Point1 = AnchorPoint;
			PendingMoveLine->Point2 = hitLoc;
			PendingMoveLine->Color = FColor::Black;
			PendingMoveLine->Thickness = 3.0f;
		}
		else
		{
			return false;
		}

		switch (Controller->EMPlayerState->SnappedCursor.SnapType)
		{
			case ESnapType::CT_WORLDSNAPX:
			case ESnapType::CT_WORLDSNAPY:
			case ESnapType::CT_WORLDSNAPZ:
			case ESnapType::CT_WORLDSNAPXY:
			case ESnapType::CT_USERPOINTSNAP:
			case ESnapType::CT_CUSTOMSNAPX:
			case ESnapType::CT_CUSTOMSNAPY:
			case ESnapType::CT_CUSTOMSNAPZ:
				PendingMoveLine->SetActorHiddenInGame(true);
				break;
			default:
				PendingMoveLine->SetActorHiddenInGame(false);
				break;
		};

		Controller->UpdateDimensionString(hitLoc, AnchorPoint, Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

		for (auto &kvp : OriginalObjectData)
		{
			kvp.Key->SetFromDataRecordAndDisplacement(kvp.Value, hitLoc - AnchorPoint);
		}
	}
	return true;
}

bool UMoveObjectTool::HandleInputNumber(double n)
{
	if (Controller->EMPlayerState->SnappedCursor.Visible)
	{
		FVector direction = (Controller->EMPlayerState->SnappedCursor.WorldPosition - AnchorPoint).GetSafeNormal();
		FVector oldPos = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		Controller->EMPlayerState->SnappedCursor.WorldPosition = AnchorPoint + direction * n;
		EndUse();
		Controller->EMPlayerState->SnappedCursor.WorldPosition = oldPos;
	}
	return true;
}

bool UMoveObjectTool::EndUse()
{
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	if (Controller->EMPlayerState->SnappedCursor.Visible)
	{
		RestoreSelectedObjects();

		TArray<int32> ids;
		ids.Reset(OriginalObjectData.Num());
		for (auto &kvp : OriginalObjectData)
		{
			ids.Add(kvp.Key->ID);
		}
		ReleaseSelectedObjects();

		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		Controller->ModumateCommand(
			FModumateCommand(Commands::kMoveObjects)
			.Param(Parameters::kDelta, hitLoc - AnchorPoint)
			.Param(Parameters::kObjectIDs, ids));
	}

	if (PendingMoveLine.IsValid())
	{
		PendingMoveLine->Destroy();
	}
	ReleaseSelectedObjects();

	return Super::EndUse();
}

bool UMoveObjectTool::AbortUse()
{
	RestoreSelectedObjects();
	ReleaseSelectedObjects();

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	if (PendingMoveLine.IsValid())
	{
		PendingMoveLine->Destroy();
	}

	return UEditModelToolBase::AbortUse();
}

bool UMoveObjectTool::HandleControlKey(bool pressed)
{
	return true;
}

