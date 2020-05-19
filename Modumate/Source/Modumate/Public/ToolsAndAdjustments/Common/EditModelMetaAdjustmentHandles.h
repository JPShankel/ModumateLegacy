// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"


namespace Modumate
{
	class MODUMATE_API FAdjustMetaEdgeHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		/*TArray<FVector> OriginalP;
		FVector AnchorLoc;
		TArray<FVector> LastValidPendingCPLocations;
		FVector HandleOriginalPoint;
		FVector HandleCurrentPoint;*/

		FVector OriginalEdgeStart, OriginalEdgeEnd, OriginalEdgeCenter, EdgeDir;
		int32 StartVertexID, EndVertexID;
		const class FGraph3D *VolumeGraph;
		FPlane EdgeConstraintPlane;

		bool CalculateConstraintPlaneFromVertices();

	public:

		FAdjustMetaEdgeHandle(FModumateObjectInstance *moi);

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual bool OnEndUse() override;
		virtual bool OnAbortUse() override;
		virtual FVector GetAttachmentPoint() override;
		virtual bool HandleInputNumber(float number) override;
	};
}