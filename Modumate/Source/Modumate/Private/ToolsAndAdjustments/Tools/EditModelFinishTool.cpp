// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelFinishTool.h"

#include "ModumateObjectInstance.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "ModumateObjectDatabase.h"
#include "ModumateObjectStatics.h"
#include "ModumateDocument.h"
#include "ModumateCommands.h"

using namespace Modumate;

UFinishTool::UFinishTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }

bool UFinishTool::Activate()
{
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;

	LastValidTarget = nullptr;
	LastValidHitActor.Reset();
	LastValidHitLocation = LastValidHitNormal = FVector::ZeroVector;
	LastValidFaceIndex = INDEX_NONE;

	return UEditModelToolBase::Activate();
}

bool UFinishTool::Deactivate()
{
	if (Controller)
	{
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	}

	return UEditModelToolBase::Deactivate();
}

bool UFinishTool::BeginUse()
{
	Super::BeginUse();

	auto *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	if (gameState && LastValidTarget && (LastValidTarget->ID != 0) && (LastValidFaceIndex != INDEX_NONE))
	{
		Controller->ModumateCommand(
			FModumateCommand(Commands::kAddFinish)
			.Param(Parameters::kObjectID, LastValidTarget->ID)
			.Param(Parameters::kIndex, LastValidFaceIndex)
			.Param(Parameters::kAssembly, Assembly.Key)
		);
	}

	EndUse();
	return true;
}

bool UFinishTool::EnterNextStage()
{
	Super::EnterNextStage();
	return false;
}

bool UFinishTool::EndUse()
{
	Super::EndUse();
	return true;
}

bool UFinishTool::AbortUse()
{
	Super::AbortUse();
	return true;
}

bool UFinishTool::FrameUpdate()
{
	Super::FrameUpdate();

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	if (cursor.Actor && (cursor.SnapType == ESnapType::CT_FACESELECT))
	{
		if (!LastValidTarget || (cursor.Actor != LastValidHitActor.Get()) || !cursor.HitNormal.Equals(LastValidHitNormal))
		{
			LastValidTarget = nullptr;
			LastValidHitActor.Reset();

			auto *gameState = Cast<AEditModelGameState_CPP>(Controller->GetWorld()->GetGameState());
			if (auto *moi = gameState->Document.ObjectFromActor(cursor.Actor))
			{
				LastValidFaceIndex = UModumateObjectStatics::GetFaceIndexFromTargetHit(moi, cursor.WorldPosition, cursor.HitNormal);

				if (LastValidFaceIndex != INDEX_NONE)
				{
					LastValidTarget = moi;
					LastValidHitActor = cursor.Actor;
					LastValidHitNormal = cursor.HitNormal;
				}
			}
		}

		FVector faceNormal;
		if (LastValidTarget && UModumateObjectStatics::GetGeometryFromFaceIndex(LastValidTarget, LastValidFaceIndex, LastCornerIndices, faceNormal))
		{
			int32 numCorners = LastCornerIndices.Num();
			for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
			{
				int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

				FVector curCornerPos = LastValidTarget->GetCorner(LastCornerIndices[curCornerIdx]);
				FVector nextCornerPos = LastValidTarget->GetCorner(LastCornerIndices[nextCornerIdx]);

				Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(
					curCornerPos, nextCornerPos, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
				);
			}
		}
	}
	else
	{
		LastValidTarget = nullptr;
	}

	return true;
}


