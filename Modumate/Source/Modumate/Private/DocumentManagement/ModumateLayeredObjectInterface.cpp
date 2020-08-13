// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateLayeredObjectInterface.h"
#include "DocumentManagement/ModumateObjectInstance.h"

namespace Modumate
{
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

	void FCachedLayerDimsByType::UpdateLayersFromAssembly(const FBIMAssemblySpec &Assembly)
	{
		// reset layer dimensions
		NumLayers = Assembly.Layers.Num();
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
			const FBIMLayerSpec &layer = Assembly.Layers[layerIdx];
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
			const FBIMLayerSpec &layer = Assembly.Layers[layerIdx];
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

	void FCachedLayerDimsByType::UpdateFinishFromObject(const FModumateObjectInstance* MOI)
	{
		bHasStartFinish = bHasEndFinish = false;
		StartFinishThickness = EndFinishThickness = 0.0f;
		TArray<const FModumateObjectInstance *> children = MOI->GetChildObjects();

		for (const FModumateObjectInstance *child : children)
		{
			if (child && (child->GetObjectType() == EObjectType::OTFinish) && ensureAlways(child->GetControlPointIndices().Num() == 1))
			{
				float finishThickness = child->CalculateThickness();
				int32 finishSideIdx = child->GetControlPointIndex(0);

				if (finishSideIdx == 0)
				{
					bHasEndFinish = true;
					EndFinishThickness = finishThickness;
				}
				else if (finishSideIdx == 1)
				{
					bHasStartFinish = true;
					StartFinishThickness = finishThickness;
				}
			}
		}

		TotalFinishedWidth = TotalUnfinishedWidth + StartFinishThickness + EndFinishThickness;
	}
}
