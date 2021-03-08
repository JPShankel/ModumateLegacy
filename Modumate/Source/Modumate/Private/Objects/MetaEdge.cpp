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

		// If this edge has a detail whose condition no longer applies to this edge's current status, then delete the detail as a side effect.
		if (CachedEdgeDetailMOI && (CachedEdgeDetailMOI->GetAssembly().EdgeDetailData.CachedConditionHash != CachedEdgeDetailConditionHash) && OutSideEffectDeltas)
		{
			auto deleteObjectDelta = MakeShared<FMOIDelta>();
			deleteObjectDelta->AddCreateDestroyState(CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
			OutSideEffectDeltas->Add(deleteObjectDelta);
			ResetEdgeDetail();
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

		UpdateVisuals();
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
	// TODO: enable for shipping builds when edge detail serialization is stable and backed by BIM
#if !UE_BUILD_SHIPPING

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
		if (false)
		{
			edgeDetailField->ButtonPressedEvent.AddDynamic(this, &AMOIMetaEdge::OnInstPropEdgeDetailButtonPress);
		}
	}
#endif
}

const FMiterData& AMOIMetaEdge::GetMiterData() const
{
	return CachedMiterData;
}

void AMOIMetaEdge::ResetEdgeDetail()
{
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

	FEdgeDetailData currentEdgeDetail(GetMiterInterface());
	CachedEdgeDetailConditionHash = currentEdgeDetail.CachedConditionHash;
}

void AMOIMetaEdge::OnInstPropEdgeDetailButtonPress(EEdgeDetailWidgetActions EdgeDetailAction)
{
	switch (EdgeDetailAction)
	{
	case EEdgeDetailWidgetActions::Swap:
	{
		auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
		controller->EditModelUserWidget->SelectionTrayWidget->StartDetailDesignerFromSelection();

		if (false)//CachedEdgeDetailMOI == nullptr)
		{
			auto& presetCollection = Document->GetPresetCollection();
			FEdgeDetailData testDetailData;
			TArray<FGuid> existingPresets;
			EBIMResult searchResult = presetCollection.GetPresetsByPredicate([this, &testDetailData](const FBIMPresetInstance& Preset) {
				return Preset.CustomData.LoadStructData(testDetailData, true) && (testDetailData.CachedConditionHash == CachedEdgeDetailConditionHash); },
				existingPresets);
			if ((searchResult == EBIMResult::Success) && (existingPresets.Num() > 0))
			{
				UE_LOG(LogTemp, Log, TEXT("There are %d other edge detail presets with the same condition!"), existingPresets.Num());
			}

			FEdgeDetailData newDetailData(GetMiterInterface());
			FBIMPresetInstance newDetailPreset;
			if ((presetCollection.GetBlankPresetForObjectType(EObjectType::OTEdgeDetail, newDetailPreset) == EBIMResult::Success) &&
				newDetailPreset.CustomData.SaveStructData(newDetailData))
			{
				TArray<FDeltaPtr> deltas;

				auto makedetailPresetDelta = presetCollection.MakeCreateNewDelta(newDetailPreset);
				deltas.Add(makedetailPresetDelta);

				auto makeDetailMOIDelta = MakeShared<FMOIDelta>();
				FMOIStateData newDetailState(Document->GetNextAvailableID(), EObjectType::OTEdgeDetail, ID);
				newDetailState.AssemblyGUID = newDetailPreset.GUID;
				makeDetailMOIDelta->AddCreateDestroyState(newDetailState, EMOIDeltaType::Create);
				deltas.Add(makeDetailMOIDelta);

				Document->ApplyDeltas(deltas, GetWorld());
			}
		}
	}
		break;
	case EEdgeDetailWidgetActions::Edit:
	{
		auto& presetCollection = Document->GetPresetCollection();
		FBIMPresetInstance* currentDetailPreset = presetCollection.PresetFromGUID(CachedEdgeDetailDataID);
		if (CachedEdgeDetailMOI && currentDetailPreset)
		{
			const FEdgeDetailData& curEdgeDetailData = CachedEdgeDetailMOI->GetAssembly().EdgeDetailData;
			const FString& detailName = currentDetailPreset->DisplayName.ToString();

			TArray<int32> orientationIndices;
			FEdgeDetailData testDetailData;
			TArray<FGuid> existingPresets;
			EBIMResult searchResult = presetCollection.GetPresetsByPredicate([this, &testDetailData](const FBIMPresetInstance& Preset) {
				return (CachedEdgeDetailDataID != Preset.GUID) && Preset.CustomData.LoadStructData(testDetailData, true) && (testDetailData.CachedConditionHash == CachedEdgeDetailConditionHash); },
				existingPresets);
			int32 numMatchingPresets = existingPresets.Num();
			UE_LOG(LogTemp, Log, TEXT("There are %d other edge detail presets with the same condition!"), numMatchingPresets);

			if ((searchResult == EBIMResult::Success) && (numMatchingPresets > 0))
			{
				FBIMAssemblySpec otherDetailAssembly;
				for (auto& otherDetailGUID : existingPresets)
				{
					FBIMPresetInstance* otherDetailPreset = presetCollection.PresetFromGUID(otherDetailGUID);
					if (otherDetailPreset &&
						presetCollection.TryGetProjectAssemblyForPreset(EObjectType::OTEdgeDetail, otherDetailGUID, otherDetailAssembly) &&
						curEdgeDetailData.CompareConditions(otherDetailAssembly.EdgeDetailData, orientationIndices))
					{
						UE_LOG(LogTemp, Log, TEXT("Edge Detail #%d (hash %08X, preset \"%s\") matches Edge Detail Preset \"%s\" with orientations: [%s]"),
							CachedEdgeDetailMOI->ID, CachedEdgeDetailConditionHash, *detailName, *otherDetailPreset->DisplayName.ToString(),
							*FString::JoinBy(orientationIndices, TEXT(", "), [](const int32& o) {return FString::Printf(TEXT("%d"), o); }));
					}
				}
			}

			auto metaEdges = Document->GetObjectsOfType(EObjectType::OTMetaEdge);
			for (auto* moi : metaEdges)
			{
				auto miterNode = moi ? moi->GetMiterInterface() : nullptr;
				if (miterNode && (moi != this))
				{
					FEdgeDetailData otherEdgeDetailData(miterNode);
					if (curEdgeDetailData.CompareConditions(otherEdgeDetailData, orientationIndices))
					{
						UE_LOG(LogTemp, Log, TEXT("Edge Detail #%d (hash %08X, preset \"%s\") matched by Edge #%d conditions with orientations: [%s]"),
							CachedEdgeDetailMOI->ID, CachedEdgeDetailConditionHash, *detailName, moi->ID,
							*FString::JoinBy(orientationIndices, TEXT(", "), [](const int32& o) {return FString::Printf(TEXT("%d"), o); }));
					}
				}
			}
		}
	}
		break;
#if 0
	case EEdgeDetailWidgetActions::Delete:
		if (CachedEdgeDetailMOI != nullptr)
		{
			auto deleteDetailDelta = MakeShared<FMOIDelta>();
			deleteDetailDelta->AddCreateDestroyState(CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
			Document->ApplyDeltas({ deleteDetailDelta }, GetWorld());

			// TODO: delete the edge detail preset itself, and propagate that change to any other edges that may share this detail
		}
		break;
#endif
	default:
		break;
	}
}
