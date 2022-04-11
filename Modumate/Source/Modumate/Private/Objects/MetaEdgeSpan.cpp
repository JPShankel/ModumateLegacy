// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdgeSpan.h"
#include "DocumentManagement/ModumateDocument.h"

AMOIMetaEdgeSpan::AMOIMetaEdgeSpan()
	: AMOIEdgeBase()
{
}

FVector AMOIMetaEdgeSpan::GetLocation() const
{
	return Super::GetLocation();
}

FVector AMOIMetaEdgeSpan::GetCorner(int32 index) const
{
	if(CachedGraphEdge && CachedGraphEdge->bValid)
	{
		int32 vertID = index == 0 ? CachedGraphEdge->StartVertexID : CachedGraphEdge->EndVertexID;
		const auto* graph = Document->GetVolumeGraph(Document->FindGraph3DByObjID(vertID));
		const auto* vertex = graph ? graph->FindVertex(vertID) : nullptr;
		if(vertex)
		{
			return vertex->Position;
		}
	}
	return FVector::ZeroVector;
}

bool AMOIMetaEdgeSpan::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		// When a span loses its all its hosted objects, delete it
		if (GetChildIDs().Num() == 0 || InstanceData.GraphMembers.Num()==0)
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
	return AMOIEdgeBase::CleanObject(DirtyFlag, OutSideEffectDeltas);
}

bool AMOIMetaEdgeSpan::TryUpdateCachedGraphData()
{
	if (InstanceData.GraphMembers.Num() > 0)
	{
		// Single face only
		int32 curEdgeID = InstanceData.GraphMembers[0];
		auto* graph = GetDocument()->FindVolumeGraph(curEdgeID);
		CachedGraphEdge = graph ? graph->FindEdge(curEdgeID) : nullptr;
		return (ensure(CachedGraphEdge != nullptr));
	}
	return false;
}
