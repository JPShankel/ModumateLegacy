// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelFinishTool.h"

#include "DocumentManagement/ModumateObjectInstance.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateCommands.h"

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
		const FModumateObjectAssembly *assembly = gameState->GetAssemblyByKey_DEPRECATED(EToolMode::VE_FINISH, Assembly.Key);
		const FModumateObjectInstance *parentMOI = gameState->Document.GetObjectById(LastValidTarget->ID);

		// If we're replacing an existing finish, just swap its assembly
		if (ensureAlways(assembly != nullptr && parentMOI != nullptr))
		{
			for (auto &childID : parentMOI->GetChildren())
			{
				FModumateObjectInstance *child = gameState->Document.GetObjectById(childID);
				if (ensureAlways(child != nullptr))
				{
					if (child->GetObjectType() == EObjectType::OTFinish)
					{
						int32 existingFinishFace = UModumateObjectStatics::GetFaceIndexFromFinishObj(child);

						if (existingFinishFace != LastValidFaceIndex)
						{
							continue;
						}

						child->BeginPreviewOperation();
						child->SetAssembly(*assembly);

						TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta({ child }));
						child->EndPreviewOperation();

						FModumateFunctionParameterSet result = Controller->ModumateCommand(delta->AsCommand());

						EndUse();
						return result.GetValue(Parameters::kSuccess);
					}
				}
			}

			FMOIStateData stateData;
			stateData.StateType = EMOIDeltaType::Create;
			stateData.ObjectType = EObjectType::OTFinish;
			stateData.ObjectAssemblyKey = Assembly.Key;
			stateData.ParentID = LastValidTarget->ID;
			stateData.ControlIndices = { LastValidFaceIndex };
			stateData.ObjectID = gameState->Document.GetNextAvailableID();

			TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta({ stateData }));
			Controller->ModumateCommand(delta->AsCommand());
		}
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


