// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlaneSpan.h"
#include "DocumentManagement/ModumateDocument.h"

AMOIMetaPlaneSpan::AMOIMetaPlaneSpan()
	: AMOIPlaneBase()
{
}

FVector AMOIMetaPlaneSpan::GetLocation() const
{
	return Super::GetLocation();
}

bool AMOIMetaPlaneSpan::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		// When a span loses its all its hosted objects, delete it
		if (GetChildIDs().Num() == 0 || InstanceData.GraphMembers.Num() == 0)
		{
			TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
			delta->AddCreateDestroyState(StateData, EMOIDeltaType::Destroy);
			OutSideEffectDeltas->Add(delta);
		}
		else
		{
			TryUpdateCachedGraphData();
		}
	}
	return AMOIPlaneBase::CleanObject(DirtyFlag, OutSideEffectDeltas);
}

bool AMOIMetaPlaneSpan::TryUpdateCachedGraphData()
{
	if (InstanceData.GraphMembers.Num() > 0)
	{
		// Single face only
		int32 curFaceID = InstanceData.GraphMembers[0];
		auto* graph = GetDocument()->FindVolumeGraph(curFaceID);
		CachedGraphFace = graph ? graph->FindFace(curFaceID) : nullptr;

		if (ensure(CachedGraphFace && (CachedGraphFace->CachedPositions.Num() > 0)))
		{
			CachedPoints = CachedGraphFace->CachedPositions;
			CachedPlane = CachedGraphFace->CachedPlane;
			CachedAxisX = CachedGraphFace->Cached2DX;
			CachedAxisY = CachedGraphFace->Cached2DY;
			CachedOrigin = CachedGraphFace->CachedPositions[0];
			CachedCenter = CachedGraphFace->CachedCenter;
			CachedHoles = CachedGraphFace->CachedHoles;
			return true;
		}
	}

	return false;
}
