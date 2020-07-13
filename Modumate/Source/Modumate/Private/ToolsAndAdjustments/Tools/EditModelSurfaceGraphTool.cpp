// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"

#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

using namespace Modumate;

USurfaceGraphTool::USurfaceGraphTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USurfaceGraphTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;

	ResetTarget();

	return true;
}

bool USurfaceGraphTool::Deactivate()
{
	if (Controller)
	{
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	}

	return Super::Deactivate();
}

bool USurfaceGraphTool::BeginUse()
{
	if (!Super::BeginUse())
	{
		return false;
	}

	auto *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();

	if (gameState && LastHostTarget && (LastHostTarget->ID != 0) && (LastValidFaceIndex != INDEX_NONE) && (LastGraphTarget == nullptr))
	{
		const FModumateObjectInstance *hostMOI = gameState->Document.GetObjectById(LastHostTarget->ID);

		if (ensureAlways(hostMOI))
		{
			TArray<TSharedPtr<FDelta>> deltas;

			int32 surfaceGraphID = gameState->Document.GetNextAvailableID();

			TSharedPtr<FGraph2DDelta> addGraphDelta = MakeShareable(new FGraph2DDelta());
			addGraphDelta->ID = surfaceGraphID;
			addGraphDelta->DeltaType = EGraph2DDeltaType::Add;
			deltas.Add(addGraphDelta);

			FMOIStateData surfaceObjectData;
			surfaceObjectData.StateType = EMOIDeltaType::Create;
			surfaceObjectData.ObjectType = EObjectType::OTSurfaceGraph;
			surfaceObjectData.ObjectAssemblyKey = NAME_None;
			surfaceObjectData.ParentID = LastHostTarget->ID;
			surfaceObjectData.ControlIndices = { LastValidFaceIndex };
			surfaceObjectData.ObjectID = surfaceGraphID;

			TSharedPtr<FMOIDelta> addObjectDelta = MakeShareable(new FMOIDelta({ surfaceObjectData }));
			deltas.Add(addObjectDelta);

			gameState->Document.ApplyDeltas(deltas, GetWorld());
		}
	}

	EndUse();
	return true;
}

bool USurfaceGraphTool::EnterNextStage()
{
	return Super::EnterNextStage();
}

bool USurfaceGraphTool::EndUse()
{
	return Super::EndUse();
}

bool USurfaceGraphTool::AbortUse()
{
	return Super::AbortUse();
}

bool USurfaceGraphTool::FrameUpdate()
{
	auto *gameState = Cast<AEditModelGameState_CPP>(Controller->GetWorld()->GetGameState());
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	ResetTarget();

	if (gameState && cursor.Actor && (cursor.SnapType == ESnapType::CT_FACESELECT))
	{
		if (auto *moi = gameState->Document.ObjectFromActor(cursor.Actor))
		{
			LastValidFaceIndex = UModumateObjectStatics::GetFaceIndexFromTargetHit(moi, cursor.WorldPosition, cursor.HitNormal);

			if (LastValidFaceIndex != INDEX_NONE)
			{
				LastHostTarget = moi;
				LastHitHostActor = cursor.Actor;
				LastValidHitNormal = cursor.HitNormal;

				// See if there's already a surface graph mounted to the same target MOI, at the same target face as the current target.
				for (int32 childID : LastHostTarget->GetChildIDs())
				{
					FModumateObjectInstance *childObj = gameState->Document.GetObjectById(childID);
					int32 childFaceIdx = UModumateObjectStatics::GetParentFaceIndex(childObj);
					if (childObj && (childObj->GetObjectType() == EObjectType::OTSurfaceGraph) && (childFaceIdx == LastValidFaceIndex))
					{
						LastGraphTarget = childObj;
						break;
					}
				}
			}
		}

		FVector faceNormal;
		if (LastHostTarget && (LastGraphTarget == nullptr) &&
			UModumateObjectStatics::GetGeometryFromFaceIndex(LastHostTarget, LastValidFaceIndex, LastCornerIndices, faceNormal))
		{
			int32 numCorners = LastCornerIndices.Num();
			for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
			{
				int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

				FVector curCornerPos = LastHostTarget->GetCorner(LastCornerIndices[curCornerIdx]);
				FVector nextCornerPos = LastHostTarget->GetCorner(LastCornerIndices[nextCornerIdx]);

				Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(
					curCornerPos, nextCornerPos, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
				);
			}
		}
	}

	return true;
}

void USurfaceGraphTool::ResetTarget()
{
	LastHostTarget = nullptr;
	LastGraphTarget = nullptr;
	LastHitHostActor.Reset();
	LastValidHitLocation = LastValidHitNormal = FVector::ZeroVector;
	LastValidFaceIndex = INDEX_NONE;
}
