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
		auto vertex = Document->GetVolumeGraph()->FindVertex(vertID);
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
		if (GetChildIDs().Num() == 0)
		{
			TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
			delta->AddCreateDestroyState(StateData, EMOIDeltaType::Destroy);
			OutSideEffectDeltas->Add(delta);
		}
	}
	return AMOIEdgeBase::CleanObject(DirtyFlag, OutSideEffectDeltas);
}

void AMOIMetaEdgeSpan::SetupDynamicGeometry()
{
	TryUpdateCachedGraphData();
}

void AMOIMetaEdgeSpan::MakeMetaEdgeSpanDeltaPtr(UModumateDocument* Doc, int32 NewID, const TArray<int32>& InMemberObjects, TSharedPtr<FMOIDelta>& OutMoiDeltaPtr)
{
	FMOIStateData newSpanStateData;
	newSpanStateData.ID = NewID;
	newSpanStateData.ObjectType = EObjectType::OTMetaEdgeSpan;

	FMOIMetaEdgeSpanData newSpanCustomData;
	for (const auto curID : InMemberObjects)
	{
		const auto volumeGraph = Doc->FindVolumeGraph(curID);
		if (volumeGraph)
		{
			const auto edge = volumeGraph->FindEdge(curID);
			if (edge)
			{
				newSpanCustomData.GraphMembers.Add(edge->ID);
			}
		}
	}
	newSpanStateData.CustomData.SaveStructData<FMOIMetaEdgeSpanData>(newSpanCustomData);
	OutMoiDeltaPtr->AddCreateDestroyState(newSpanStateData, EMOIDeltaType::Create);
}

void AMOIMetaEdgeSpan::MakeMetaEdgeSpanDeltaFromGraph(FGraph3D* InGraph, int32 NewID, const TArray<int32>& InMemberObjects, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	FMOIStateData newSpanStateData;
	newSpanStateData.ID = NewID;
	newSpanStateData.ObjectType = EObjectType::OTMetaEdgeSpan;

	FMOIMetaEdgeSpanData newSpanCustomData;
	for (const auto curID : InMemberObjects)
	{
		const auto edge = InGraph->FindEdge(curID);
		if (edge)
		{
			newSpanCustomData.GraphMembers.Add(edge->ID);
		}
	}
	newSpanStateData.CustomData.SaveStructData<FMOIMetaEdgeSpanData>(newSpanCustomData);

	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(newSpanStateData, EMOIDeltaType::Create);
	OutDeltaPtrs.Add(delta);
}

bool AMOIMetaEdgeSpan::TryUpdateCachedGraphData()
{
	if (InstanceData.GraphMembers.Num() < 2)
	{
		// Single face only
		int32 curEdgeID = InstanceData.GraphMembers[0];
		auto* graph = GetDocument()->FindVolumeGraph(curEdgeID);
		CachedGraphEdge = graph ? graph->FindEdge(curEdgeID) : nullptr;
	}
	else
	{
		// After two edges are join, the new face will be used to join the following edges
		int32 newEdgeID = InstanceData.GraphMembers[0];
		int32 docNextID = Document->GetNextAvailableID();
		for (int32 i = 0; i < InstanceData.GraphMembers.Num() - 1; ++i)
		{
			TArray<int32> objPair = { newEdgeID, InstanceData.GraphMembers[i + 1] };
			TArray<FGraph3DDelta> graphDeltas;
			if (!Document->GetTempVolumeGraph().GetDeltasForSpanJoin(graphDeltas, objPair, docNextID, EGraph3DObjectType::Edge))
			{
				FGraph3D::CloneFromGraph(Document->GetTempVolumeGraph(), *Document->GetVolumeGraph());
				return false;
			}

			// Find the new face to be used for next joining
			for (auto curDelta : graphDeltas)
			{
				for (auto kvp : curDelta.EdgeAdditions)
				{
					newEdgeID = kvp.Key;
				}
			}
		}

		FGraph3DEdge* newEdge = Document->GetTempVolumeGraph().FindEdge(newEdgeID);
		if (newEdge && newEdge->bValid)
		{
			CachedGraphEdge = newEdge;
		}
	}

	if (ensure(CachedGraphEdge))
	{
		return true;
	}
	return false;
}
