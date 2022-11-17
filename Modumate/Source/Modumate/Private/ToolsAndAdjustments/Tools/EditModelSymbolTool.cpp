// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSymbolTool.h"

#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelGameState.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "Objects/ModumateSymbolDeltaStatics.h"
#include "Objects/MetaGraph.h"

USymbolTool::USymbolTool()
{ }

bool USymbolTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	Controller->DeselectAll();
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	OnAssemblyChanged();

	return true;
}

bool USymbolTool::BeginUse()
{
	Super::BeginUse();
	EndUse();

	if (!SymbolGuid.IsValid())
	{
		return true;
	}

	GameState->Document->ClearPreviewDeltas(GetWorld());
	TArray<FDeltaPtr> deltas;
	const FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	const FVector location = snappedCursor.WorldPosition;

	bool bRetVal = false;
	if (GetObjectCreationDeltas(location - SymbolAnchor, true, deltas))
	{
		bRetVal = GameState->Document->ApplyDeltas(deltas, GetWorld());
		OnAssemblyChanged();
	}

	return bRetVal;
}

// Symbol ID rather than Assembly.
void USymbolTool::OnAssemblyChanged()
{
	const auto* doc = GameState->Document;
	if (!ensure(doc))
	{
		return;
	}

	const auto& presets = doc->GetPresetCollection();
	SymbolGuid = GetAssemblyGUID();
	const FBIMPresetInstance* symbolPreset = presets.PresetFromGUID(SymbolGuid);
	if (symbolPreset)
	{
		FBIMSymbolPresetData symbolData;

		// Similar to UPasteTool, find a minimal anchor point.
		if (ensure(symbolPreset->TryGetCustomData(symbolData)))
		{
			SymbolAnchor = symbolData.Anchor;
		}
	}
}

bool USymbolTool::GetObjectCreationDeltas(const FVector& Location, bool bPresetDelta, TArray<FDeltaPtr>& OutDeltas)
{
	FBIMSymbolPresetData symbolData;
	const FBIMPresetInstance* symbolPreset = GameState->Document->GetPresetCollection().PresetFromGUID(SymbolGuid);
	FBIMSymbolCollectionProxy symbolCollection(&GameState->Document->GetPresetCollection());
	if (symbolCollection.PresetDataFromGUID(SymbolGuid))
	{
		auto* doc = GameState->Document;
		int32 nextID = doc->GetNextAvailableID();
		int32 newGroupID = nextID++;

		// New Group MOI + graph3d
		auto groupDelta = MakeShared<FMOIDelta>();
		FMOIStateData newGroupState(newGroupID, EObjectType::OTMetaGraph, doc->GetActiveVolumeGraphID());
		FMOIMetaGraphData newGroupData;
		newGroupData.Location = Location;
		newGroupState.CustomData.SaveStructData(newGroupData, UE_EDITOR);
		newGroupState.AssemblyGUID = SymbolGuid;
		groupDelta->AddCreateDestroyState(newGroupState, EMOIDeltaType::Create);
		auto newGraph3d = MakeShared<FGraph3DDelta>(newGroupID);
		newGraph3d->DeltaType = EGraph3DDeltaType::Add;
		OutDeltas.Add(newGraph3d);
		OutDeltas.Add(groupDelta);

		TSet<int32> unused;
		FModumateSymbolDeltaStatics::CreateDeltasForNewSymbolInstance(doc, newGroupID, nextID, SymbolGuid, symbolCollection,
			FTransform(Location), OutDeltas, {SymbolGuid}, unused);

		if (bPresetDelta)
		{
			symbolCollection.GetPresetDeltas(OutDeltas);
		}
	}
	return true;
}

bool USymbolTool::Deactivate()
{
	Super::Deactivate();

	SymbolGuid.Invalidate();
	return true;
}

bool USymbolTool::FrameUpdate()
{
	if (!SymbolGuid.IsValid())
	{
		return true;
	}

	TArray<FDeltaPtr> deltas;
	const FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	const FVector location = snappedCursor.WorldPosition;

	if (GetObjectCreationDeltas(location - SymbolAnchor, false, deltas))
	{
		GameState->Document->ApplyPreviewDeltas(deltas, GetWorld());
	}

	return true;
}
