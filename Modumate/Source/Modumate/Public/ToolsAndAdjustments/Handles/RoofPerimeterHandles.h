// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelAdjustmentHandleBase.h"

namespace Modumate
{
	class MODUMATE_API FCreateRoofFacesHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FCreateRoofFacesHandle(FModumateObjectInstance *MOI)
			: FEditModelAdjustmentHandleBase(MOI)
		{ }

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;
	};

	class MODUMATE_API FRetractRoofFacesHandle : public FEditModelAdjustmentHandleBase
	{
	public:
		FRetractRoofFacesHandle(FModumateObjectInstance *MOI)
			: FEditModelAdjustmentHandleBase(MOI)
		{ }

		virtual bool OnBeginUse() override;
		virtual FVector GetAttachmentPoint() override;
	};
}
