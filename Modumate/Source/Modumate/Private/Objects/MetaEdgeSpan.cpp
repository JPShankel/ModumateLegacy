// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdgeSpan.h"
#include "DocumentManagement/ModumateDocument.h"

AMOIMetaEdgeSpan::AMOIMetaEdgeSpan()
	: AMOIEdgeBase()
{
}

FVector AMOIMetaEdgeSpan::GetCorner(int32 index) const
{
	int32 vertID = index == 0 ? CachedGraphEdge.StartVertexID : CachedGraphEdge.EndVertexID;
	const auto* graph = Document->GetVolumeGraph(Document->FindGraph3DByObjID(vertID));
	const auto* vertex = graph ? graph->FindVertex(vertID) : nullptr;
	if(vertex)
	{
		return vertex->Position;
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
			UpdateCachedEdge();
		}
	}
	return AMOIEdgeBase::CleanObject(DirtyFlag, OutSideEffectDeltas);
}

void AMOIMetaEdgeSpan::SetupDynamicGeometry()
{
	UpdateCachedEdge();
}

bool AMOIMetaEdgeSpan::UpdateCachedEdge()
{

	if (InstanceData.GraphMembers.Num() == 0)
	{
		return false;
	}

	auto* graph = GetDocument()->FindVolumeGraph(InstanceData.GraphMembers[0]);

	TMap<int32, int32> vertCounts;

	for (auto& edgeID : InstanceData.GraphMembers)
	{
		const FGraph3DEdge* edgeOb = graph->FindEdge(edgeID);
		if (edgeOb)
		{
			int32 count = vertCounts.FindOrAdd(edgeOb->StartVertexID, 0);
			++count;
			vertCounts.Add(edgeOb->StartVertexID, count);

			count = vertCounts.FindOrAdd(edgeOb->EndVertexID, 0);
			++count;
			vertCounts.Add(edgeOb->EndVertexID, count);

			CachedGraphEdge.CachedRefNorm = edgeOb->CachedRefNorm;
		}
	}

	TArray<int32> outVerts;

	for (auto& kvp : vertCounts)
	{
		if (kvp.Value == 1)
		{
			outVerts.Add(kvp.Key);
		}
	}

	if (outVerts.Num() >= 2)
	{
		CachedGraphEdge.StartVertexID = outVerts[0];
		CachedGraphEdge.EndVertexID = outVerts[1];

		const FGraph3DVertex* v1 = graph->FindVertex(CachedGraphEdge.StartVertexID);
		const FGraph3DVertex* v2 = graph->FindVertex(CachedGraphEdge.EndVertexID);

		if (v1 && v2)
		{
			CachedGraphEdge.CachedMidpoint = (v1->Position + v2->Position) * 0.5f;
		}
	}

	return false;
}
