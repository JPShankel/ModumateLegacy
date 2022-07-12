// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelFFETool.h"

#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Objects/ModumateObjectStatics.h"
#include "ModumateCore/ModumateTargetingStatics.h"
#include "Objects/FFE.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelDatasmithImporter.h"



UFFETool::UFFETool()
	: Super()
{}

bool UFFETool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	// Set default settings for NewMOIStateData
	FMOIFFEData newFFECustomData;
	NewMOIStateData.ObjectType = EObjectType::OTFurniture;
	NewMOIStateData.CustomData.SaveStructData(newFFECustomData);

	Controller->DeselectAll();
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	ResetState();

	return true;
}

bool UFFETool::ScrollToolOption(int32 dir)
{
	return false;
}

bool UFFETool::Deactivate()
{
	ResetState();
	Super::Deactivate();

	return true;
}

bool UFFETool::FrameUpdate()
{
	const FBIMAssemblySpec* assemblySpec = GameState->Document->GetPresetCollection().GetAssemblyByGUID(EToolMode::VE_PLACEOBJECT, AssemblyGUID);
	if (!assemblySpec)
	{
		return false;
	}

	const FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	AModumateObjectInstance* hitMOI = GameState->Document->ObjectFromActor(snappedCursor.Actor);
	if (hitMOI && IsObjectInActiveGroup(hitMOI))
	{
		LastParentID = hitMOI->ID;
	}
	else
	{
		LastParentID = MOD_ID_NONE;
	}

	FTransform mountedTransform;
	if (!UModumateObjectStatics::GetFFEMountedTransform(assemblySpec, snappedCursor, mountedTransform))
	{
		return false;
	}
	TargetLocation = mountedTransform.GetLocation();
	TargetRotation = mountedTransform.GetRotation();
	if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas(LastParentID, TargetLocation, TargetRotation, CurDeltas))
	{
		GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
	}
	return true;
}

bool UFFETool::BeginUse()
{
	if (!AssemblyGUID.IsValid())
	{
		return false;
	}

	Super::BeginUse();
	Super::EndUse();

	GameState->Document->ClearPreviewDeltas(GetWorld());

	TArray<FDeltaPtr> deltas;
	if (!GetObjectCreationDeltas(LastParentID, TargetLocation, TargetRotation, deltas))
	{
		return false;
	}

	if (deltas.Num() == 0)
	{
		return false;
	}

	bool bSuccess = GameState->Document->ApplyDeltas(deltas, GetWorld());

	EndUse();

	return bSuccess;
}

void UFFETool::ResetState()
{
	LastParentID = MOD_ID_NONE;
	TargetLocation = FVector::ZeroVector;
	TargetRotation = FQuat::Identity;
}

bool UFFETool::GetObjectCreationDeltas(const int32 InLastParentID, const FVector& InLocation, const FQuat& InRotation, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutDeltaPtrs.Reset();
	NewObjectIDs.Reset();

	TSharedPtr<FMOIDelta> delta;
	int32 nextID = GameState->Document->GetNextAvailableID();

	if (!delta.IsValid())
	{
		delta = MakeShared<FMOIDelta>();
		OutDeltaPtrs.Add(delta);
	}

	FMOIFFEData newFFECustomData;
	newFFECustomData.Location = InLocation;
	newFFECustomData.Rotation = InRotation;
	NewMOIStateData.CustomData.SaveStructData(newFFECustomData);

	NewMOIStateData.ID = nextID++;
	NewMOIStateData.ParentID = InLastParentID;
	NewMOIStateData.AssemblyGUID = AssemblyGUID;
	NewObjectIDs.Add(NewMOIStateData.ID);

	delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
	return true;
}
