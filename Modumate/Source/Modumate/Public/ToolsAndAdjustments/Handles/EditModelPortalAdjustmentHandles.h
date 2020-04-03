// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "EditModelAdjustmentHandleBase.h"
#include "AdjustmentHandleActor_CPP.h"
#include "DynamicMeshActor.h"

namespace Modumate
{
	class MODUMATE_API FAdjustPortalSideHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		FVector PlanePoint, PlaneNormal, AnchorPoint;
		int32 Side;
		TArray<FVector> BeginUpdateHolePonts;
		FVector HoleActorBeginLocation;

	public:

		FAdjustPortalSideHandle(FModumateObjectInstance *moi, int32 side)
			: FEditModelAdjustmentHandleBase(moi)
			, PlanePoint(ForceInitToZero)
			, PlaneNormal(ForceInitToZero)
			, AnchorPoint(ForceInitToZero)
			, Side(side)
			, HoleActorBeginLocation(ForceInitToZero)
		{ }

		virtual ~FAdjustPortalSideHandle() {}
		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual bool OnEndUse() override;
		virtual bool OnAbortUse() override;
		virtual FVector GetAttachmentPoint() override;
		virtual bool HandleInputNumber(float number) override;

	};

	class MODUMATE_API FAdjustPortalInvertHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		float Sign;

	public:

		FAdjustPortalInvertHandle(FModumateObjectInstance *moi, float sign = 1.f) :
			FEditModelAdjustmentHandleBase(moi), Sign(sign)
		{}

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;
	};

	class MODUMATE_API FAdjustPortalPointHandle : public FEditModelAdjustmentHandleBase
	{
	private:
		TArray<int32> CP;
		TArray<FVector> OriginalP;
		FVector AnchorLoc;
		FVector OrginalLoc;
		FVector OriginalLocHandle;
		TArray<FVector> LastValidPendingCPLocations;
		int HandleID = 0;

	public:
		FAdjustPortalPointHandle(FModumateObjectInstance *moi, int cp) :
			FEditModelAdjustmentHandleBase(moi)
		{
			CP.Add(cp);
			HandleID = cp;
		}

		virtual bool OnBeginUse() override;
		virtual bool OnUpdateUse() override;
		virtual bool OnEndUse() override;
		virtual bool OnAbortUse() override;
		virtual FVector GetAttachmentPoint() override;
		virtual bool HandleInputNumber(float number) override;
	};
}
