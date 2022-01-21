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



UMoveObjectTool::UMoveObjectTool()
	: Super()
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

				// Move entire groups
				for (auto& kvp : OriginalGroupVertexTransforms)
				{
					FTransform newTransform(kvp.Value);
					newTransform.AddToTranslation(offset);
					objectInfo.Add(kvp.Key, newTransform);
				}
				TArray<FDeltaPtr> deltas;
				GetDeltasForGraphMoves(doc, objectInfo, deltas);

				objectInfo.Reset();
				for (auto& kvp : OriginalTransforms)
				{
					objectInfo.Add(kvp.Key, FTransform(kvp.Value.GetRotation(), kvp.Value.GetTranslation() + offset));;
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
					GetDeltasForGroupCopies(doc, offset, groupDeltas);
					doc->ApplyPreviewDeltas(groupDeltas, GetWorld());
				}
				FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, doc, Controller, true, &groupDeltas);
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

			doc->ClearPreviewDeltas(GetWorld());
			if (!bPaste && !Controller->IsControlDown())
			{
				TMap<int32, FTransform> objectInfo;

				// Move entire groups
				for (auto& kvp : OriginalGroupVertexTransforms)
				{
					FTransform newTransform(kvp.Value);
					newTransform.AddToTranslation(offset);
					objectInfo.Add(kvp.Key, newTransform);
				}
				TArray<FDeltaPtr> deltas;
				GetDeltasForGraphMoves(doc, objectInfo, deltas);
				objectInfo.Reset();

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
				GetDeltasForGroupCopies(doc, offset, groupDeltas);
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
			TMap<int32, FTransform> objectInfo;
			for (auto& kvp : OriginalGroupVertexTransforms)
			{
				AModumateObjectInstance* targetMOI = doc->GetObjectById(kvp.Key);
				if (ensure(targetMOI))
				{
					objectInfo.Add(kvp.Key, targetMOI->GetWorldTransform());
				}
			}

			TArray<FDeltaPtr> deltas;
			GetDeltasForGraphMoves(doc, objectInfo, deltas);

			ReleaseObjectsAndApplyDeltas(&deltas);
		}

		return Super::EndUse();
	}
	else
	{
		GameState->Document->ClearPreviewDeltas(GetWorld());

		FVector offset(snappedCursor.WorldPosition - AnchorPoint);
		TArray<FDeltaPtr> groupDeltas;
		GetDeltasForGroupCopies(doc, offset, groupDeltas);
		doc->BeginUndoRedoMacro();
		doc->ApplyDeltas(groupDeltas, GetWorld());

		FModumateObjectDeltaStatics::PasteObjects(&CurrentRecord, AnchorPoint, GameState->Document, Controller, false);
		doc->EndUndoRedoMacro();

		snappedCursor.SetAffordanceFrame(AnchorPoint, snappedCursor.HitNormal, snappedCursor.HitTangent);
		// Re-acquire for next copy.
		OriginalTransforms.Empty();
		OriginalGroupVertexTransforms.Empty();
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

void UMoveObjectTool::GetDeltasForGraphMoves(UModumateDocument* Doc, const TMap<int32, FTransform>& Transforms, TArray<FDeltaPtr>& OutDeltas)
{
	// Create deltas directly for vertex moves rather than use routines that finalize the deltas
	// since it is a graph block move.
	for (const auto& kvp : Transforms)
	{
		int32 id = kvp.Key;
		int32 graphId = Doc->FindGraph3DByObjID(id);
		FGraph3D* graph = Doc->GetVolumeGraph(graphId);
		const FGraph3DVertex* vertex = graph ? graph->GetVertices().Find(id) : nullptr;
		if (ensure(vertex))
		{
			FGraph3DDelta delta(graphId);
			delta.VertexMovements.Add(id, FModumateVectorPair(OriginalGroupVertexTransforms[kvp.Key].GetTranslation(), kvp.Value.GetTranslation()) );
			OutDeltas.Add(MakeShared<FGraph3DDelta>(delta));
		}
	}
}

// Copy all the new-graph deltas shifting the vertices by Offset.
void UMoveObjectTool::GetDeltasForGroupCopies(UModumateDocument* Doc, FVector Offset, TArray<FDeltaPtr>& OutDeltas)
{
	for (const auto& deltaPair: GroupCopyDeltas)
	{
		FDeltaPtr delta = deltaPair.Value;
		if (deltaPair.Key)
		{
			FGraph3DDelta graphDelta(*static_cast<FGraph3DDelta*>(delta.Get()));
			for (auto& kvp : graphDelta.VertexAdditions)
			{
				kvp.Value += Offset;
			}
			OutDeltas.Add(MakeShared<FGraph3DDelta>(graphDelta));
		}
		else
		{
			OutDeltas.Add(delta);
		}
	}
}
