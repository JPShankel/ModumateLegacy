// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdge.h"

#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MiterNode.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UI/EditModelUserWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"


AMOIMetaEdge::AMOIMetaEdge()
	: AMOIEdgeBase()
	, CachedEdgeDetailMOI(nullptr)
	, CachedEdgeDetailData(FEdgeDetailData::CurrentVersion)
	, CachedEdgeDetailDataID()
	, CachedEdgeDetailConditionHash(0)
	, BaseDefaultColor(FColor::Black)
	, BaseGroupedColor(FColor(0x0D, 0x0B, 0x55))
	, HoverDefaultColor(FColor::Black)
	, HoverGroupedColor(FColor(0x0D, 0x0B, 0x55))
{
}

bool AMOIMetaEdge::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	UModumateDocument* doc = GetDocument();

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		GetConnectedMOIs(CachedConnectedMOIs);

		auto& graph = doc->GetVolumeGraph();
		auto edge = graph.FindEdge(ID);
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
		MarkDirty(EObjectDirtyFlags::Visuals | EObjectDirtyFlags::Mitering);
	}
	break;
	case EObjectDirtyFlags::Mitering:
	{
		// clean the miter flag by performing the mitering for this edge's connected plane-hosted objects
		if (!CachedMiterData.GatherDetails(this))
		{
			return false;
		}

		CacheEdgeDetail();

		// If this edge has a detail, we may need to update it if conditions have changed.
		if (CachedEdgeDetailMOI && OutSideEffectDeltas)
		{
			// If the current orientation is no longer valid, see if there is another valid one.
			TArray<int32> tempDetailOrientations;
			if (CachedEdgeDetailMOI->GetAssembly().EdgeDetailData.CompareConditions(CachedEdgeDetailData, tempDetailOrientations) && (tempDetailOrientations.Num() > 0))
			{
				if (!tempDetailOrientations.Contains(CachedEdgeDetailMOI->InstanceData.OrientationIndex))
				{
					auto updateOrientationDelta = MakeShared<FMOIDelta>();
					auto& newStateData = updateOrientationDelta->AddMutationState(CachedEdgeDetailMOI);
					FMOIEdgeDetailData newDetailInstanceData = CachedEdgeDetailMOI->InstanceData;
					newDetailInstanceData.OrientationIndex = tempDetailOrientations[0];
					if (ensure(newStateData.CustomData.SaveStructData(newDetailInstanceData)))
					{
						OutSideEffectDeltas->Add(updateOrientationDelta);
						ResetEdgeDetail();
					}
				}
			}
			// If there is no valid orientation for the saved detail, then delete the detail.
			else
			{
				auto deleteObjectDelta = MakeShared<FMOIDelta>();
				deleteObjectDelta->AddCreateDestroyState(CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
				OutSideEffectDeltas->Add(deleteObjectDelta);
				ResetEdgeDetail();
			}
		}

		CachedMiterData.CalculateMitering();

		// If the miter participants aren't already miter-dirty, then mark them dirty now so that they can update their layer extensions.
		for (auto& kvp : CachedMiterData.ParticipantsByID)
		{
			AModumateObjectInstance* miterParticipantMOI = doc->GetObjectById(kvp.Key);
			if (miterParticipantMOI)
			{
				miterParticipantMOI->MarkDirty(EObjectDirtyFlags::Mitering);
			}
		}
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		for (AModumateObjectInstance* connectedMOI : CachedConnectedMOIs)
		{
			if ((connectedMOI->GetObjectType() == EObjectType::OTMetaPlane) && connectedMOI->IsDirty(EObjectDirtyFlags::Visuals))
			{
				return false;
			}
		}

		return TryUpdateVisuals();
	}
	break;
	default:
		break;
	}

	return true;
}

void AMOIMetaEdge::ShowAdjustmentHandles(AEditModelPlayerController* Controller, bool bShow)
{
	AMOIEdgeBase::ShowAdjustmentHandles(Controller, bShow);

	UModumateDocument* doc = GetDocument();
	if (!ensure(doc))
	{
		return;
	}

	auto& graph = doc->GetVolumeGraph();
	auto edge = graph.FindEdge(ID);
	if (IsDestroyed() || !ensure(edge))
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

void AMOIMetaEdge::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI)
{
	// If there are no miter participants, then the edge cannot have a detail
	// (and if it did, it should have been deleted)
	if (CachedMiterData.SortedMiterIDs.Num() == 0)
	{
		return;
	}

	static const FString edgeDetailPropertyName(TEXT("Detail"));
	if (auto edgeDetailField = PropertiesUI->RequestPropertyField<UInstPropWidgetEdgeDetail>(this, edgeDetailPropertyName))
	{
		edgeDetailField->RegisterValue(this, ID, CachedEdgeDetailDataID, CachedEdgeDetailConditionHash);
	}
}

const FMiterData& AMOIMetaEdge::GetMiterData() const
{
	return CachedMiterData;
}

void AMOIMetaEdge::ResetEdgeDetail()
{
	CachedEdgeDetailData.Reset();
	CachedEdgeDetailDataID.Invalidate();
	CachedEdgeDetailMOI = nullptr;
	CachedEdgeDetailConditionHash = 0;
}

void AMOIMetaEdge::CacheEdgeDetail()
{
	ResetEdgeDetail();

	for (int32 childID : CachedChildIDs)
	{
		auto childDetailObj = Cast<AMOIEdgeDetail>(Document->GetObjectById(childID));
		if (childDetailObj && ensureMsgf(CachedEdgeDetailMOI == nullptr, TEXT("Massing Edge #%d has more than one child Edge Detail!"), ID))
		{
			CachedEdgeDetailMOI = childDetailObj;
		}
	}

	if (CachedEdgeDetailMOI)
	{
		CachedEdgeDetailDataID = CachedEdgeDetailMOI->GetAssembly().UniqueKey();
	}

	CachedEdgeDetailData.FillFromMiterNode(GetMiterInterface());
	CachedEdgeDetailConditionHash = CachedEdgeDetailData.CachedConditionHash;
}
