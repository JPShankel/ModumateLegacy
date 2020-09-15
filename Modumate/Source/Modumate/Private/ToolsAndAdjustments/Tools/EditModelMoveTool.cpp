#include "ToolsAndAdjustments/Tools/EditModelMoveTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/LineActor.h"

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
		PendingSegmentID = DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass())->ID;

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

		ALineActor *pendingSegment = nullptr;
		if (auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID))
		{
			pendingSegment = dimensionActor->GetLineActor();
		}
		if (pendingSegment != nullptr)
		{
			pendingSegment->Point1 = AnchorPoint;
			pendingSegment->Point2 = hitLoc;
			pendingSegment->Color = FColor::Black;
			pendingSegment->Thickness = 3.0f;
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
				pendingSegment->SetActorHiddenInGame(true);
				break;
			default:
				pendingSegment->SetActorHiddenInGame(false);
				break;
		};

		FModumateDocument* doc = Controller->GetDocument();
		if (doc != nullptr)
		{
			FVector offset = hitLoc - AnchorPoint;
			TMap<int32, FTransform> objectInfo;
			for (auto& kvp : OriginalTransforms)
			{
				objectInfo.Add(kvp.Key, FTransform(kvp.Value.GetRotation(), kvp.Value.GetTranslation() + offset));;
			}

			if (!FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), true))
			{
				return false;
			}
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
		ReleaseObjectsAndApplyDeltas();
	}

	DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	PendingSegmentID = MOD_ID_NONE;

	return Super::EndUse();
}

bool UMoveObjectTool::AbortUse()
{
	ReleaseSelectedObjects();

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	PendingSegmentID = MOD_ID_NONE;

	return UEditModelToolBase::AbortUse();
}

bool UMoveObjectTool::HandleControlKey(bool pressed)
{
	return true;
}

