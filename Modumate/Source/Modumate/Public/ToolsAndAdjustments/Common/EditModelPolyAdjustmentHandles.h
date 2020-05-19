// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"


namespace Modumate
{
	class MODUMATE_API FAdjustPolyPointHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		TArray<int32> CP;
		TArray<FVector> OriginalP;
		FVector AnchorLoc;
		TArray<FVector> LastValidPendingCPLocations;
		FVector HandleOriginalPoint;
		FVector HandleCurrentPoint;

	public:

		FAdjustPolyPointHandle(FModumateObjectInstance *moi, int32 cp) :
			FEditModelAdjustmentHandleBase(moi)
		{
			CP.Add(cp);
		}

		FAdjustPolyPointHandle(FModumateObjectInstance *moi, int32 cp, int32 cp2) :
			FEditModelAdjustmentHandleBase(moi)
		{
			CP.Add(cp);
			CP.Add(cp2);
		}

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual FVector GetAttachmentPoint() override;
		virtual bool HandleInputNumber(float number) override;
	};

	class MODUMATE_API FAdjustPolyExtrusionHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		float Sign;
		TArray<FVector> OriginalControlPoints;
		FPlane OriginalPlane;
		float OriginalExtrusion;
		float LastValidExtrusion;
		FVector AnchorLoc;

	public:

		FAdjustPolyExtrusionHandle(FModumateObjectInstance *moi, float sign = 1.0f)
			: FEditModelAdjustmentHandleBase(moi)
			, Sign(sign)
		{ }

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual FVector GetAttachmentPoint() override;
		virtual bool HandleInputNumber(float number) override;
	};

	class MODUMATE_API FAdjustInvertHandle : public FEditModelAdjustmentHandleBase
	{
	public:

		FAdjustInvertHandle(FModumateObjectInstance *moi) :
			FEditModelAdjustmentHandleBase(moi)
		{}

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;
	};

	class MODUMATE_API FJustificationHandle : public FEditModelAdjustmentHandleBase
	{
	public:

		FJustificationHandle(FModumateObjectInstance *moi) :
			FEditModelAdjustmentHandleBase(moi)
		{}

		float LayerPercentage = 0.f;

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;
	};
}