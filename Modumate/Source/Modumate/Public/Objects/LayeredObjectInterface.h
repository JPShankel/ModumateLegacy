// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"

class AModumateObjectInstance;
class UModumateDocument;

enum class EMiterLayerGroupPreferredNeighbor
{
	Previous,
	Next,
	Both
};

FORCEINLINE EMiterLayerGroupPreferredNeighbor MiterLayerGroupOtherNeighbor(EMiterLayerGroupPreferredNeighbor Neighbor)
{
	switch (Neighbor)
	{
	case EMiterLayerGroupPreferredNeighbor::Previous: return EMiterLayerGroupPreferredNeighbor::Next;
	case EMiterLayerGroupPreferredNeighbor::Next: return EMiterLayerGroupPreferredNeighbor::Previous;
	};

	return EMiterLayerGroupPreferredNeighbor::Both;
}

struct MODUMATE_API FMiterLayerGroup
{
	int32 StartIndex = INDEX_NONE, 
		EndIndex = INDEX_NONE, 
		ParticipantIdx = INDEX_NONE, 
		GroupIdx = INDEX_NONE;

	EMiterLayerGroupPreferredNeighbor PreferredNeighbor;
	
	float GroupThickness = 0.0f, GroupOffset = 0.0f;

	bool bIsLoneTopPriorityLayer = false;

	FBIMPresetLayerPriority Priority;
};

struct MODUMATE_API FCachedLayerDimsByType
{
	int32 NumLayers = 0;

	bool bHasStartFinish = false;
	float StartFinishThickness = 0.0f;
	bool bHasEndFinish = false;
	float EndFinishThickness = 0.0f;

	float TotalUnfinishedWidth = 0.0f;
	float TotalFinishedWidth = 0.0f;

	TArray<float, TInlineAllocator<8>> LayerOffsets;
	TArray<float, TInlineAllocator<8>> LayerThicknesses;

	TArray<FMiterLayerGroup> LayerGroups;

	void UpdateLayersFromAssembly(const UModumateDocument* Document, const FBIMAssemblySpec& Assembly);
	void UpdateLayersFromAssembly(const UModumateDocument* Document, const TArray<FBIMLayerSpec>& AssemblyLayers);
	void UpdateFinishFromObject(const AModumateObjectInstance *MOI);

	private:

	TArray<FBIMPresetLayerPriority> NormalizedLayerPriorities;
	
	void CalcNormalizeLayerPriorities(const TArray<FBIMLayerSpec>& AssemblyLayers, FBIMPresetLayerPriority& OutHighestPriority);
};

class MODUMATE_API ILayeredObject
{
public:
	virtual const FCachedLayerDimsByType &GetCachedLayerDims() const = 0;
};
