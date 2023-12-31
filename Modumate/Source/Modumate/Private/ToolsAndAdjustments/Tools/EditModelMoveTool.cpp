#include "ToolsAndAdjustments/Tools/EditModelMoveTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "Objects/MetaGraph.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/LineActor.h"



UMoveObjectTool::UMoveObjectTool()
	: Super()
	, FSelectedObjectToolMixin(Controller)
{}

bool UMoveObjectTool::Activate()
{
	Super::Activate();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	// Release previous stored values before activating
	if (OriginalTransforms.Num() ||
		OriginalGroupTransforms.Num() ||
		OriginalSelectedObjects.Num() ||
		OriginalSelectedGroupObjects.Num() ||
		GroupCopyDeltas.Num())
	{
		ReleaseSelectedObjects();
	}
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

	const FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	if (IsInUse() && snappedCursor.Visible)
	{
		const FVector &hitLoc = snappedCursor.WorldPosition;

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

		switch (snappedCursor.SnapType)
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

				// Move entire groups
				TArray<FDeltaPtr> deltas;
				FModumateObjectDeltaStatics::GetDeltasForGroupTransforms(doc, OriginalGroupTransforms, FTransform(offset),
					deltas);

				for (auto& kvp : OriginalTransforms)
				{
					objectInfo.Add(kvp.Key, FTransform(kvp.Value.GetRotation(), kvp.Value.GetTranslation() + offset));
				}

				if (!FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), true, &deltas))
				{
					return false;
				}

			}
			else
			{
				TArray<FDeltaPtr> groupDeltas;
				if (GroupCopyDeltas.Num() > 0)
				{
					GetDeltasForGroupCopies(doc, offset, groupDeltas, false);
				}
				FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, doc, Controller, true, &groupDeltas);
			}
		}

		UpdateEdgeDimension(snappedCursor);

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

			doc->ClearPreviewDeltas(GetWorld());
			if (!bPaste && !Controller->IsControlDown())
			{
				TMap<int32, FTransform> objectInfo;

				// Move entire groups
				TArray<FDeltaPtr> deltas;
				FModumateObjectDeltaStatics::GetDeltasForGroupTransforms(doc, OriginalGroupTransforms, FTransform(offset),
					deltas);

				for (auto& kvp : OriginalTransforms)
				{
					objectInfo.Add(kvp.Key, FTransform(kvp.Value.GetRotation(), kvp.Value.GetTranslation() + offset));;
				}

				if (!FModumateObjectDeltaStatics::MoveTransformableIDs(objectInfo, doc, Controller->GetWorld(), false, &deltas))
				{
					return false;
				}

			}
			else
			{
				TArray<FDeltaPtr> groupDeltas;
				GetDeltasForGroupCopies(doc, offset, groupDeltas, true);
				doc->BeginUndoRedoMacro();
				doc->ApplyDeltas(groupDeltas, GetWorld());

				FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, doc, Controller, false);
				doc->EndUndoRedoMacro();
			}
		}
	}

	ReleaseSelectedObjects();

	return PostEndOrAbort();
}

bool UMoveObjectTool::EndUse()
{
	FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	snappedCursor.ClearAffordanceFrame();
	snappedCursor.MouseMode = EMouseMode::Location;
	UModumateDocument* doc = Controller->GetDocument();

	if (!bPaste)
	{
		snappedCursor.WantsVerticalAffordanceSnap = false;
		if (Controller->EMPlayerState->SnappedCursor.Visible)
		{
			// Move entire groups
			TArray<FDeltaPtr> groupDeltas;

			if (OriginalGroupTransforms.Num() > 0)
			{
				AModumateObjectInstance* targetMOI = doc->GetObjectById(OriginalGroupTransforms.begin()->Key);
				if (ensure(targetMOI))
				{
					FTransform newXform(OriginalGroupTransforms.begin()->Value.Inverse()* targetMOI->GetWorldTransform());
					FModumateObjectDeltaStatics::GetDeltasForGroupTransforms(doc, OriginalGroupTransforms, newXform,
						groupDeltas);
				}
			}

			ReleaseObjectsAndApplyDeltas(&groupDeltas);
		}

		return Super::EndUse();
	}
	else
	{
		GameState->Document->ClearPreviewDeltas(GetWorld());

		FVector offset(snappedCursor.WorldPosition - AnchorPoint);
		TArray<FDeltaPtr> groupDeltas;
		GetDeltasForGroupCopies(doc, offset, groupDeltas, true);
		doc->BeginUndoRedoMacro();
		doc->ApplyDeltas(groupDeltas, GetWorld());

		FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, GameState->Document, Controller, false);
		doc->EndUndoRedoMacro();

		snappedCursor.SetAffordanceFrame(AnchorPoint, snappedCursor.HitNormal, snappedCursor.HitTangent);
		// Re-acquire for next copy.
		OriginalTransforms.Empty();
		OriginalGroupTransforms.Empty();
		OriginalSelectedObjects.Empty();
		OriginalSelectedGroupObjects.Empty();
		GroupCopyDeltas.Empty();

		AcquireSelectedObjects();
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

// Copy all the new-graph deltas shifting the vertices by Offset.
void UMoveObjectTool::GetDeltasForGroupCopies(UModumateDocument* Doc, FVector Offset, TArray<FDeltaPtr>& OutDeltas, bool bPresetsAlso)
{
	FModumateObjectDeltaStatics::GetDeltasForGroupCopies(Doc, Offset, GroupCopyDeltas, OutDeltas, bPresetsAlso);
}
