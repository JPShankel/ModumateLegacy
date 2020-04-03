// Copyright 2018 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ModumateObjectAssembly.h"

namespace Modumate
{
	class FModumateObjectInstance;

	struct FCachedLayerDimsByType
	{
		int32 NumLayers = 0;

		int32 StructuralLayerStartIdx = INDEX_NONE;
		int32 StructuralLayerEndIdx = INDEX_NONE;
		float StructureWidthStart = 0.0f;
		float StructureWidthEnd = 0.0f;

		int32 StartMembraneIdx = INDEX_NONE;
		float StartMembraneStart = 0.0f;
		float StartMembraneEnd = 0.0f;
		int32 EndMembraneIdx = INDEX_NONE;
		float EndMembraneStart = 0.0f;
		float EndMembraneEnd = 0.0f;

		bool bHasStartFinish = false;
		float StartFinishThickness = 0.0f;
		bool bHasEndFinish = false;
		float EndFinishThickness = 0.0f;

		float TotalUnfinishedWidth = 0.0f;
		float TotalFinishedWidth = 0.0f;

		TArray<float, TInlineAllocator<8>> LayerOffsets;

		bool HasStructuralLayers() const;
		bool HasStartMembrane() const;
		bool HasEndMembrane() const;
		bool HasStartFinish() const;
		bool HasEndFinish() const;

		void UpdateLayersFromAssembly(const FModumateObjectAssembly &Assembly);
		void UpdateFinishFromObject(const FModumateObjectInstance *MOI);
	};

	class MODUMATE_API ILayeredObject
	{
	public:
		virtual const FCachedLayerDimsByType &GetCachedLayerDims() const = 0;
	};
}
