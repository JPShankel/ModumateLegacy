// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdge.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/MiterNode.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"


FMOIMetaEdgeImpl::FMOIMetaEdgeImpl(FModumateObjectInstance *moi)
	: FMOIEdgeImplBase(moi)
{
}

bool FMOIMetaEdgeImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	FModumateDocument* doc = MOI->GetDocument();

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		MOI->GetConnectedMOIs(CachedConnectedMOIs);

		auto& graph = doc->GetVolumeGraph();
		auto edge = graph.FindEdge(MOI->ID);
		auto vertexStart = edge ? graph.FindVertex(edge->StartVertexID) : nullptr;
		auto vertexEnd = edge ? graph.FindVertex(edge->EndVertexID) : nullptr;
		if (!ensure(LineActor.IsValid() && vertexStart && vertexEnd))
		{
			return false;
		}

		LineActor->Point1 = vertexStart->Position;
		LineActor->Point2 = vertexEnd->Position;

		// If our own geometry has been updated, then we need to re-evaluate our mitering.
		MOI->MarkDirty(EObjectDirtyFlags::Mitering);
	}
	break;
	case EObjectDirtyFlags::Mitering:
	{
		// TODO: clean the miter details by performing the mitering for this edge's connected plane-hosted objects
		if (!CachedMiterData.GatherDetails(MOI))
		{
			return false;
		}

		CachedMiterData.CalculateMitering();

		// If the miter participants aren't already miter-dirty, then mark them dirty now so that they can update their layer extensions.
		for (auto& kvp : CachedMiterData.ParticipantsByID)
		{
			FModumateObjectInstance* miterParticipantMOI = doc->GetObjectById(kvp.Key);
			if (miterParticipantMOI)
			{
				miterParticipantMOI->MarkDirty(EObjectDirtyFlags::Mitering);
			}
		}
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		for (FModumateObjectInstance* connectedMOI : CachedConnectedMOIs)
		{
			if ((connectedMOI->GetObjectType() == EObjectType::OTMetaPlane) && connectedMOI->IsDirty(EObjectDirtyFlags::Visuals))
			{
				return false;
			}
		}

		MOI->UpdateVisibilityAndCollision();
	}
	break;
	default:
		break;
	}

	return true;
}

void FMOIMetaEdgeImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller)
{
	if (MOI->HasAdjustmentHandles())
	{
		return;
	}

	// Edges always have two vertices
	for (int32 i = 0; i < 2; ++i)
	{
		auto vertexHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		vertexHandle->SetTargetIndex(i);
	}
}

const FMiterData& FMOIMetaEdgeImpl::GetMiterData() const
{
	return CachedMiterData;
}

