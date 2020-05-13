// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "RoofPerimeterHandles.h"

namespace Modumate
{
	bool FCreateRoofFacesHandle::OnBeginUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("Use roof create faces handle on roof #%d"), MOI->ID);

		OnEndUse();
		return false;
	}

	FVector FCreateRoofFacesHandle::GetAttachmentPoint()
	{
		return MOI->GetObjectLocation();
	}


	bool FRetractRoofFacesHandle::OnBeginUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("Use roof retract faces handle on roof #%d"), MOI->ID);

		OnEndUse();
		return false;
	}

	FVector FRetractRoofFacesHandle::GetAttachmentPoint()
	{
		return MOI->GetObjectLocation();
	}
}
