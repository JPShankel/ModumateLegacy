// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/EdgeDetailObj.h"
#include "DocumentManagement/ModumateDocument.h"


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

bool FMOITypicalEdgeDetailDelta::ApplyTo(UModumateDocument* Doc, UWorld* World) const 
{
	if (NewTypicalEdge.IsValid())
	{
		Doc->TypicalEdgeDetails.Add(ConditionValue,NewTypicalEdge);
	}
	else
	{
		Doc->TypicalEdgeDetails.Remove(ConditionValue);
	}
	return true;
}

TSharedPtr<FDocumentDelta> FMOITypicalEdgeDetailDelta::MakeInverse() const 
{
	TSharedPtr<FMOITypicalEdgeDetailDelta> inverse = MakeShared<FMOITypicalEdgeDetailDelta>();
	inverse->OldTypicalEdge = NewTypicalEdge;
	inverse->NewTypicalEdge = OldTypicalEdge;
	inverse->ConditionValue = ConditionValue;
	return inverse;
}

FStructDataWrapper FMOITypicalEdgeDetailDelta::SerializeStruct() 
{
	return FStructDataWrapper(StaticStruct(), this, true);
}

