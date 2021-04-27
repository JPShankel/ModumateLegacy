#include "ToolsAndAdjustments/Tools/EditModelMoveTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
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
	bCtrlIsPressed = false;

	AcquireSelectedObjects();

	if (Controller->EMPlayerState->SnappedCursor.Visible)
	{
		AnchorPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorPoint, Controller->EMPlayerState->SnappedCursor.HitNormal, Controller->EMPlayerState->SnappedCursor.HitTangent);

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

		UModumateDocument* doc = Controller->GetDocument();
		if (doc != nullptr)
		{
			if (Controller->IsControlDown())
			{
				if (!bCtrlIsPressed)
				{
					ToggleIsPasting();
					bCtrlIsPressed = true;
				}
			}
			else
			{
				bCtrlIsPressed = false;
			}

			FVector offset = hitLoc - AnchorPoint;
			if (!bPaste)
			{
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
			else
			{
				FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, doc, Controller, true);
			}
		}

	}
	return true;
}

bool UMoveObjectTool::HandleInputNumber(double n)
{
	if (Controller->EMPlayerState->SnappedCursor.Visible)
	{
		const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		UModumateDocument* doc = Controller->GetDocument();
		if (doc != nullptr)
		{
			FVector direction = hitLoc - AnchorPoint;
			direction.Normalize();
			FVector offset = direction * n;
			Controller->EMPlayerState->SnappedCursor.WorldPosition = AnchorPoint + offset;

			if (!bPaste && !Controller->IsControlDown())
			{
				TMap<int32, FTransform> objectInfo;
				for (auto& kvp : OriginalTransforms)
				{
					objectInfo.Add(kvp.Key, FTransform(kvp.Value.GetRotation(), kvp.Value.GetTranslation() + offset));;
				}

				if (!FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), false))
				{
					return false;
				}
			}
			else
			{
				doc->ClearPreviewDeltas(doc->GetWorld());
				FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, doc, Controller, false);
			}
		}
	}

	ReleaseSelectedObjects();

	return PostEndOrAbort();
}

bool UMoveObjectTool::EndUse()
{
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	if (!bPaste)
	{
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
		if (Controller->EMPlayerState->SnappedCursor.Visible)
		{
			ReleaseObjectsAndApplyDeltas();
		}

		return Super::EndUse();
	}
	else
	{
		GameState->Document->ClearPreviewDeltas(GetWorld());
		FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, GameState->Document, Controller, false);

		AnchorPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorPoint, Controller->EMPlayerState->SnappedCursor.HitNormal, Controller->EMPlayerState->SnappedCursor.HitTangent);
	}

	return true;
}

bool UMoveObjectTool::AbortUse()
{
	ReleaseSelectedObjects();

	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

	return Super::AbortUse();
}

bool UMoveObjectTool::PostEndOrAbort()
{
	bPaste = false;
	return Super::PostEndOrAbort();
}
