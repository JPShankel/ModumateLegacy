// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/LayeredObjectInterface.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "DocumentManagement/ModumateDocument.h"

// define this to produce a single structural group that will create a "triangle of mystery"
//#define NAIVE_MITERING

void FCachedLayerDimsByType::UpdateLayersFromAssembly(const UModumateDocument* Document, const TArray<FBIMLayerSpec>& AssemblyLayers)
{
	// reset layer dimensions
	NumLayers = AssemblyLayers.Num();

	EndFinishThickness = StartFinishThickness = TotalUnfinishedWidth = 0.0f;

	LayerOffsets.Reset();
	LayerThicknesses.Reset();

	if (!ensureAlways(NumLayers > 0))
	{
		return;
	}

	const FBIMPresetCollection& presetCollection = Document->GetPresetCollection();

	// Find the locations of the structural and membrane layer(s) of this assembly
	// First, traverse forwards for the start layers
	float curThickness = 0.0f;

	LayerGroups.Empty();

	FBIMPresetLayerPriority highestPriority;
	CalcNormalizeLayerPriorities(AssemblyLayers, highestPriority);

	FMiterLayerGroup* layerGroup = &LayerGroups.AddDefaulted_GetRef();
	layerGroup->StartIndex = 0;
	layerGroup->EndIndex = INDEX_NONE;
	layerGroup->Priority = NormalizedLayerPriorities[0];
	layerGroup->GroupOffset = 0.0f;
	
	EMiterLayerGroupPreferredNeighbor preferredNeighbor = layerGroup->PreferredNeighbor = 
		layerGroup->Priority == highestPriority ? EMiterLayerGroupPreferredNeighbor::Both : EMiterLayerGroupPreferredNeighbor::Previous;

	for (int32 layerIdx = 0; layerIdx < NumLayers; ++layerIdx)
	{
#ifndef NAIVE_MITERING
		// Low priority neighbors near start layer prefer the 'Prev' participant, near end layer prefer 'Next'
		// High priority neighbors prefer 'Both'
		if (layerGroup->Priority != NormalizedLayerPriorities[layerIdx])
		{
			if (preferredNeighbor == EMiterLayerGroupPreferredNeighbor::Previous && NormalizedLayerPriorities[layerIdx] == highestPriority)
			{
				preferredNeighbor = EMiterLayerGroupPreferredNeighbor::Both;
			}
			else if (preferredNeighbor == EMiterLayerGroupPreferredNeighbor::Both && NormalizedLayerPriorities[layerIdx] != highestPriority)
			{
				preferredNeighbor = EMiterLayerGroupPreferredNeighbor::Next;
			}

			layerGroup->EndIndex = layerIdx - 1;
			layerGroup = &LayerGroups.AddDefaulted_GetRef();
			layerGroup->GroupOffset = curThickness;
			layerGroup->StartIndex = layerIdx;
			layerGroup->EndIndex = INDEX_NONE;
			layerGroup->Priority = NormalizedLayerPriorities[layerIdx];
			layerGroup->PreferredNeighbor = preferredNeighbor;
		}
#endif

		LayerOffsets.Add(curThickness);
		float layerThickness = AssemblyLayers[layerIdx].ThicknessCentimeters;
		layerGroup->GroupThickness += layerThickness;
		LayerThicknesses.Add(layerThickness);
		curThickness += layerThickness;
	}

	layerGroup->EndIndex = NumLayers - 1;

	LayerOffsets.Add(curThickness);
	TotalUnfinishedWidth = curThickness;
}

/*
* For mitering to work, the priority of layers must be like a bell curve, with high priority in the center fanning out to lower
* If a lower priority layer (ie an air gap or moisture membrane) is sandwiched between higher priority layers, it inherits the priority of its neighbors
*/

void FCachedLayerDimsByType::CalcNormalizeLayerPriorities(const TArray<FBIMLayerSpec>& AssemblyLayers, FBIMPresetLayerPriority& OutHighestPriority)
{
	NormalizedLayerPriorities.Empty();
	Algo::Transform(AssemblyLayers, NormalizedLayerPriorities, [](const FBIMLayerSpec& Layer) {return Layer.LayerPriority; });

	if (NormalizedLayerPriorities.Num() == 0)
	{
		return;
	}

	OutHighestPriority = NormalizedLayerPriorities[0];

	for (int32 curLayer = 1; curLayer < NormalizedLayerPriorities.Num() - 1; ++curLayer)
	{
		FBIMPresetLayerPriority leftPriority = NormalizedLayerPriorities[curLayer];
		for (int32 i = curLayer - 1; i >= 0; --i)
		{
			if (NormalizedLayerPriorities[i] < leftPriority)
			{
				leftPriority = NormalizedLayerPriorities[i];
			}
		}

		FBIMPresetLayerPriority rightPriority = NormalizedLayerPriorities[curLayer];
		for (int32 i = curLayer + 1; i < NormalizedLayerPriorities.Num(); ++i)
		{
			if (NormalizedLayerPriorities[i] < rightPriority)
			{
				rightPriority = NormalizedLayerPriorities[i];
			}
		}

		if (rightPriority < NormalizedLayerPriorities[curLayer] && leftPriority < NormalizedLayerPriorities[curLayer])
		{
			NormalizedLayerPriorities[curLayer] = FMath::Min(rightPriority, leftPriority);
		}

		OutHighestPriority = FMath::Min(OutHighestPriority, NormalizedLayerPriorities[curLayer]);
	}

	OutHighestPriority = FMath::Min(OutHighestPriority, NormalizedLayerPriorities.Last());

#ifdef NAIVE_MITERING
	for (int32 curLayer = 0; curLayer < NormalizedLayerPriorities.Num(); ++curLayer)
	{
		NormalizedLayerPriorities[curLayer] = OutHighestPriority;
	}
#endif
}

void FCachedLayerDimsByType::UpdateLayersFromAssembly(const UModumateDocument* Document, const FBIMAssemblySpec& Assembly)
{
	UpdateLayersFromAssembly(Document, Assembly.Layers);
}

void FCachedLayerDimsByType::UpdateFinishFromObject(const AModumateObjectInstance* MOI)
{
	bHasStartFinish = bHasEndFinish = false;
	StartFinishThickness = EndFinishThickness = 0.0f;

	// TODO: now that surface graphs can make inconsistent thickness of plane-hosted layered assemblies, "finish thickness" needs attention.

	TotalFinishedWidth = TotalUnfinishedWidth + StartFinishThickness + EndFinishThickness;
}

#ifdef NAIVE_MITERING
#undef NAIVE_MITERING
#endif