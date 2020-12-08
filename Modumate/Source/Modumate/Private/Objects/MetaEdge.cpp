// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdge.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/MiterNode.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"


FMOIMetaEdgeImpl::FMOIMetaEdgeImpl(FModumateObjectInstance *moi)
	: FMOIEdgeImplBase(moi)
	, BaseDefaultColor(FColor::Black)
	, BaseGroupedColor(FColor::Purple)
	, HoverDefaultColor(FColor::Black)
	, HoverGroupedColor(FColor::Magenta)
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
		LineActor->UpdateTransform();

		bool bGrouped = (edge->GroupIDs.Num() > 0);
		BaseColor = bGrouped ? BaseGroupedColor : BaseDefaultColor;
		HoveredColor = bGrouped ? HoverGroupedColor : HoverDefaultColor;

		// If our own geometry has been updated, then we need to re-evaluate our mitering,
		// and if connectivity has changed then we need to update visuals.
		MOI->MarkDirty(EObjectDirtyFlags::Visuals | EObjectDirtyFlags::Mitering);
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

void FMOIMetaEdgeImpl::ShowAdjustmentHandles(AEditModelPlayerController_CPP* Controller, bool bShow)
{
	FMOIEdgeImplBase::ShowAdjustmentHandles(Controller, bShow);

	FModumateDocument* doc = MOI ? MOI->GetDocument() : nullptr;
	if (!ensure(doc))
	{
		return;
	}

	auto& graph = doc->GetVolumeGraph();
	auto edge = graph.FindEdge(MOI->ID);
	if (MOI->IsDestroyed() || !ensure(edge))
	{
		return;
	}

	// Mirror adjustment handles of this meta edge with all group objects that it belongs to
	for (int32 groupID : edge->GroupIDs)
	{
		auto groupObj = doc->GetObjectById(groupID);
		if (groupObj)
		{
			groupObj->ShowAdjustmentHandles(Controller, bShow);
		}
	}
}

const FMiterData& FMOIMetaEdgeImpl::GetMiterData() const
{
	return CachedMiterData;
}

