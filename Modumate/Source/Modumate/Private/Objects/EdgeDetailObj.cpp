// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeDetailObj.h"


FMOIEdgeDetailData::FMOIEdgeDetailData()
{
}

FMOIEdgeDetailData::FMOIEdgeDetailData(int32 InOrientationIndex)
	: OrientationIndex(InOrientationIndex)
{
}

bool AMOIEdgeDetail::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		AModumateObjectInstance* parentEdge = GetParentObject();
		if (!ensure(parentEdge))
		{
			return false;
		}

		parentEdge->MarkDirty(EObjectDirtyFlags::Mitering);
		return true;
	}
	break;
	default:
		return true;
	}
}
