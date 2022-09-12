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

	if (!SymbolPreset)
	{
		return true;
	}

	GameState->Document->ClearPreviewDeltas(GetWorld());
	TArray<FDeltaPtr> deltas;
	const FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	const FVector location = snappedCursor.WorldPosition;

	if (GetObjectCreationDeltas(location - SymbolAnchor, true, deltas))
	{
		return GameState->Document->ApplyDeltas(deltas, GetWorld());
	}

	return false;
}

// Symbol ID rather than Assembly.
void USymbolTool::OnAssemblyChanged()
{
	const auto* doc = GameState->Document;
	if (!ensure(doc))
	{
		return;
	}

	SymbolPreset = nullptr;
	const auto& presets = doc->GetPresetCollection();
	const FBIMPresetInstance* symbolPreset = GetAssemblyGUID().IsValid() ? presets.PresetFromGUID(GetAssemblyGUID()) : nullptr;
	if (symbolPreset)
	{
		SymbolPreset = symbolPreset;
		FBIMSymbolPresetData symbolData;
		// Similar to UPasteTool, find a minimal anchor point.
		if (ensure(SymbolPreset->TryGetCustomData(symbolData)))
		{
			FVector anchor(ForceInit);
			for (const auto& vert : symbolData.Graph3d.Vertices)
			{
				FVector position = FVector(vert.Value.Position);
				if (anchor.IsZero())
				{
					anchor = position;
				}
				if ((position.X < anchor.X) ? true : (position.Y > anchor.Y) ? true : position.Z < anchor.Z)
				{
					anchor = position;
				}
			}
			SymbolAnchor = anchor;
		}
	}
}

bool USymbolTool::GetObjectCreationDeltas(const FVector& Location, bool bPresetDelta, TArray<FDeltaPtr>& OutDeltas)
{
	FBIMSymbolPresetData symbolData;
	if (SymbolPreset->TryGetCustomData(symbolData))
	{
		auto* doc = GameState->Document;
		int32 nextID = doc->GetNextAvailableID();
		int32 newGroupID = nextID++;

		// New Group MOI + graph3d
		auto groupDelta = MakeShared<FMOIDelta>();
		FMOIStateData newGroupState(newGroupID, EObjectType::OTMetaGraph, doc->GetActiveVolumeGraphID());
		FMOIMetaGraphData newGroupData;
		newGroupData.Location = Location;
		newGroupData.SymbolID = SymbolPreset->GUID;
		newGroupState.CustomData.SaveStructData(newGroupData, UE_EDITOR);
		groupDelta->AddCreateDestroyState(newGroupState, EMOIDeltaType::Create);
		auto newGraph3d = MakeShared<FGraph3DDelta>(newGroupID);
		newGraph3d->DeltaType = EGraph3DDeltaType::Add;
		OutDeltas.Add(newGraph3d);
		OutDeltas.Add(groupDelta);

		FModumateSymbolDeltaStatics::CreateDeltasForNewSymbolInstance(doc, newGroupID, nextID, symbolData, FTransform(Location),
			OutDeltas);

		if (bPresetDelta)
		{
			FBIMPresetInstance newSymbolPrest(*SymbolPreset);
			newSymbolPrest.SetCustomData(symbolData);
			OutDeltas.Add(doc->GetPresetCollection().MakeUpdateDelta(newSymbolPrest));
		}
	}
	return true;
}

bool USymbolTool::Deactivate()
{
	Super::Deactivate();

	SymbolPreset = nullptr;
	return true;
}

bool USymbolTool::FrameUpdate()
{
	if (!SymbolPreset)
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
