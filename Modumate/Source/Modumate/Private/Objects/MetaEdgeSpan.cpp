// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdgeSpan.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Kismet/KismetMathLibrary.h"

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
		if (OutSideEffectDeltas)
		{
			if (GetChildIDs().Num() == 0 || InstanceData.GraphMembers.Num() == 0)
			{
				TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
				delta->AddCreateDestroyState(StateData, EMOIDeltaType::Destroy);
				OutSideEffectDeltas->Add(delta);
				return true;
			}
			else if (!UpdateCachedEdge())
			{
				FModumateObjectDeltaStatics::GetDeltasForSpanSplit(Document, { ID }, *OutSideEffectDeltas);
				return true;
			}
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
	bool bLegal = true;
	if (InstanceData.GraphMembers.Num() == 0)
	{
		return false;
	}

	auto* graph = GetDocument()->FindVolumeGraph(InstanceData.GraphMembers[0]);

	// Use the first graph member to establish collinearity
	const FGraph3DEdge* baseEdge = graph->FindEdge(InstanceData.GraphMembers[0]);
	if (!baseEdge)
	{
		return false;
	}

	FVector baseStart=FVector::ZeroVector, baseDirection=FVector::ZeroVector;

	FGraph3DVertex* v = graph->FindVertex(baseEdge->StartVertexID);
	if (v)
	{
		baseStart = v->Position;
	}
	v = graph->FindVertex(baseEdge->EndVertexID);
	if (v)
	{
		baseDirection = (v->Position - baseStart).GetSafeNormal();
	}

	// Endpoint vertices will have only one participating edge
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

			for (int32 vertID : {edgeOb->StartVertexID, edgeOb->EndVertexID})
			{
				v = graph->FindVertex(vertID);
				if (!v || !FMath::IsNearlyEqual(UKismetMathLibrary::GetPointDistanceToLine(v->Position, baseStart, baseDirection), 0))
				{
					bLegal = false;
				}
			}

			CachedGraphEdge.CachedRefNorm = edgeOb->CachedRefNorm;
		}
	}

	TArray<int32> outVerts;

	// Find vert counts with only 1 edge...there will be two in a legal span
	for (auto& kvp : vertCounts)
	{
		if (kvp.Value == 1)
		{
			outVerts.Add(kvp.Key);
		}
	}

	//If we found exactly two endpoints and maintained collinearity, we're good
	if (outVerts.Num() == 2)
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
	else
	{
		bLegal = false;
	}

	return bLegal;
}

