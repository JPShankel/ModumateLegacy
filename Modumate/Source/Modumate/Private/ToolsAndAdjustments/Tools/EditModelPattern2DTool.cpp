// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPattern2DTool.h"
#include "Objects/MOIPattern2D.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerState.h"

UEditModelPattern2DTool::UEditModelPattern2DTool()
{}

bool UEditModelPattern2DTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	LastTarget = MOD_ID_NONE;
	return true;
}

bool UEditModelPattern2DTool::FrameUpdate()
{
	TArray<EObjectType> compatibleObjectTypes;
	compatibleObjectTypes.Add(EObjectType::OTWallSegment);
	compatibleObjectTypes.Add(EObjectType::OTFloorSegment);
	compatibleObjectTypes.Add(EObjectType::OTRoofFace);
	compatibleObjectTypes.Add(EObjectType::OTCeiling);
	compatibleObjectTypes.Add(EObjectType::OTCountertop);
	compatibleObjectTypes.Add(EObjectType::OTSystemPanel);
	compatibleObjectTypes.Add(EObjectType::OTFaceHosted);
	compatibleObjectTypes.Add(EObjectType::OTPattern2D);
	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;

	if (cursor.Actor)
	{
		const AModumateObjectInstance* hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);
		if (hitMOI && hitMOI->GetObjectType() == EObjectType::OTMetaPlane)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("LastTarget: ") + FString::FromInt(LastTarget));
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("Hit moi id: ") + FString::FromInt(hitMOI->ID));
			if (LastTarget != hitMOI->ID && GameState->Document->StartPreviewing())
			{
				LastTarget = hitMOI->ID;
			}
		}
		else if (hitMOI && (compatibleObjectTypes.Contains(hitMOI->GetObjectType())))
		{
			LastTarget = hitMOI->GetParentID();
		}
		else {
			GameState->Document->ClearPreviewDeltas(GetWorld());
			LastTarget = MOD_ID_NONE;
		}
		if (LastTarget != MOD_ID_NONE)
		{
			FDeltaPtr delta;
			// Preview delta
			if (GameState->Document->StartPreviewing() && GetCreationDelta(delta))
			{
				GameState->Document->ApplyPreviewDeltas({ delta }, GetWorld());
			}

		}
		Controller->EMPlayerState->bShowSnappedCursor = (LastTarget == MOD_ID_NONE);
	}
	return true;
}

bool UEditModelPattern2DTool::GetCreationDelta(FDeltaPtr& OutDelta)
{
	if (LastTarget == MOD_ID_NONE)
	{
		return false;
	}
	FMOIStateData stateData;
	stateData.ID = GameState->Document->GetNextAvailableID();
	stateData.ObjectType = EObjectType::OTPattern2D;
	stateData.ParentID = LastTarget;
	stateData.AssemblyGUID = AssemblyGUID;

	FMOIPattern2DData data;
	stateData.CustomData.SaveStructData(data);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);

	OutDelta = delta;

	return true;
}

bool UEditModelPattern2DTool::BeginUse()
{
	FDeltaPtr delta;
	if (GetCreationDelta(delta))
	{
		GameState->Document->ClearPreviewDeltas(GetWorld());
		GameState->Document->ApplyDeltas({ delta }, GetWorld());
	}
	return false;
}
