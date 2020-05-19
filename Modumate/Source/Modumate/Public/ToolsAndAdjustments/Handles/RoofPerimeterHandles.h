// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"

namespace Modumate
{
	class MODUMATE_API FCreateRoofFacesHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FCreateRoofFacesHandle(FModumateObjectInstance *MOI)
			: FEditModelAdjustmentHandleBase(MOI)
		{ }

		virtual bool OnBeginUse() override;
		virtual bool OnEndUse() override;
		virtual FVector GetAttachmentPoint() override;

	protected:
		TArray<FVector> EdgePoints;
		TArray<float> EdgeSlopes;
		TArray<bool> EdgesHaveFaces;
		TArray<int32> EdgeIDs;

		TArray<FVector> CombinedPolyVerts;
		TArray<int32> PolyVertIndices;
	};

	class MODUMATE_API FRetractRoofFacesHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FRetractRoofFacesHandle(FModumateObjectInstance *MOI)
			: FEditModelAdjustmentHandleBase(MOI)
		{ }

		virtual bool OnBeginUse() override;
		virtual bool OnEndUse() override;
		virtual FVector GetAttachmentPoint() override;
	};
}
