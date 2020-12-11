// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/LayeredObjectInterface.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/ModumateObjectInstance.h"

bool FCachedLayerDimsByType::HasStructuralLayers() const
{
	return (StructuralLayerStartIdx != INDEX_NONE) && (StructuralLayerEndIdx != INDEX_NONE);
}

bool FCachedLayerDimsByType::HasStartMembrane() const
{
	return (StartMembraneIdx != INDEX_NONE);
}

bool FCachedLayerDimsByType::HasEndMembrane() const
{
	return (EndMembraneIdx != INDEX_NONE);
}

bool FCachedLayerDimsByType::HasStartFinish() const
{
	return bHasStartFinish;
}

bool FCachedLayerDimsByType::HasEndFinish() const
{
	return bHasEndFinish;
}

void FCachedLayerDimsByType::UpdateLayersFromAssembly(const TArray<FBIMLayerSpec>& AssemblyLayers)
{
	// reset layer dimensions
	NumLayers = AssemblyLayers.Num();
	StructuralLayerStartIdx = StructuralLayerEndIdx = INDEX_NONE;
	StructureWidthStart = StructureWidthEnd = 0.0f;

	StartMembraneIdx = EndMembraneIdx = INDEX_NONE;
	StartMembraneStart = StartMembraneEnd = 0.0f;
	EndMembraneStart = EndMembraneEnd = 0.0f;

	EndFinishThickness = StartFinishThickness = TotalUnfinishedWidth = 0.0f;

	LayerOffsets.Reset();

	if (!ensureAlways(NumLayers > 0))
	{
		return;
	}

	// Find the locations of the structural and membrane layer(s) of this assembly
	// First, traverse forwards for the start layers
	float curThickness = 0.0f;
	for (int32 layerIdx = 0; layerIdx < NumLayers; ++layerIdx)
	{
		LayerOffsets.Add(curThickness);
		const FBIMLayerSpec &layer = AssemblyLayers[layerIdx];
		float layerThickness = layer.Thickness.AsWorldCentimeters();

		if ((layer.Function == ELayerFunction::Membrane) && (StructuralLayerStartIdx == INDEX_NONE))
		{
			StartMembraneIdx = layerIdx;
			StartMembraneStart = curThickness;
			StartMembraneEnd = curThickness + layerThickness;
		}
		else if ((layer.Function == ELayerFunction::Structure) && (StructuralLayerStartIdx == INDEX_NONE))
		{
			StructuralLayerStartIdx = layerIdx;
			StructureWidthStart = curThickness;
		}

		curThickness += layerThickness;
	}

	LayerOffsets.Add(curThickness);
	TotalUnfinishedWidth = curThickness;

	// Next, traverse backwards for the end layers
	for (int32 layerIdx = NumLayers - 1; layerIdx >= 0; --layerIdx)
	{
		const FBIMLayerSpec &layer = AssemblyLayers[layerIdx];
		float layerThickness = layer.Thickness.AsWorldCentimeters();

		if ((layer.Function == ELayerFunction::Membrane) && (StructuralLayerEndIdx == INDEX_NONE))
		{
			EndMembraneIdx = layerIdx;
			EndMembraneEnd = curThickness;
			EndMembraneStart = curThickness - layerThickness;
		}
		else if ((layer.Function == ELayerFunction::Structure) && (StructuralLayerEndIdx == INDEX_NONE))
		{
			StructuralLayerEndIdx = layerIdx;
			StructureWidthEnd = curThickness;
		}

		curThickness -= layerThickness;
	}

	// If there were no actual structural layers, consider the entire assembly to be structural
	if ((StructuralLayerStartIdx == INDEX_NONE) ||
		(StructuralLayerEndIdx == INDEX_NONE) ||
		(StructureWidthEnd == 0.0f))
	{
		StructuralLayerStartIdx = 0;
		StructuralLayerEndIdx = NumLayers - 1;
		StructureWidthStart = 0.0f;
		StructureWidthEnd = TotalUnfinishedWidth;
	}
}

void FCachedLayerDimsByType::UpdateLayersFromAssembly(const FBIMAssemblySpec& Assembly)
{
	UpdateLayersFromAssembly(Assembly.Layers);
}

void FCachedLayerDimsByType::UpdateFinishFromObject(const AModumateObjectInstance* MOI)
{
	bHasStartFinish = bHasEndFinish = false;
	StartFinishThickness = EndFinishThickness = 0.0f;

	// TODO: now that surface graphs can make inconsistent thickness of plane-hosted layered assemblies, "finish thickness" needs attention.

	TotalFinishedWidth = TotalUnfinishedWidth + StartFinishThickness + EndFinishThickness;
}

