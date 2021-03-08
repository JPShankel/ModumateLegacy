// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetEdgeDetail.h"

#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MetaEdge.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"

#define LOCTEXT_NAMESPACE "ModumateInstanceProperties"

bool UInstPropWidgetEdgeDetail::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	CurrentConditionValue = 0;

	if (!ensure(DetailName && ButtonSwap && ButtonEdit && ButtonDelete))
	{
		return false;
	}

	ButtonSwap->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedSwap);
	ButtonEdit->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedEdit);
	ButtonDelete->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedDelete);

	return true;
}

void UInstPropWidgetEdgeDetail::ResetInstProp()
{
	Super::ResetInstProp();

	ButtonPressedEvent.Clear();
	CurrentEdgeIDs.Reset();
	CurrentPresetValues.Reset();
	CurrentConditionValue = 0;
}

void UInstPropWidgetEdgeDetail::DisplayValue()
{
	int32 numDetailPresets = CurrentPresetValues.Num();

	bool bAllowSwap = bConsistentValue && (CurrentEdgeIDs.Num() > 0) && (numDetailPresets == 1);
	bool bAllowEdit = false;
	bool bAllowDelete = (CurrentEdgeIDs.Num() > 0) && (numDetailPresets > 0) &&
		((numDetailPresets > 1) || CurrentPresetValues.CreateConstIterator()->IsValid());

	FText noDetailText = LOCTEXT("EdgeDetail_None", "None");

	if (bConsistentValue)
	{
		if (numDetailPresets == 0)
		{
			DetailName->ModumateTextBlock->SetText(noDetailText);
		}
		else if (numDetailPresets == 1)
		{
			FGuid detailPresetID = *CurrentPresetValues.CreateConstIterator();
			if (detailPresetID.IsValid())
			{
				auto controller = GetOwningPlayer<AEditModelPlayerController>();
				FBIMPresetInstance* detailPresetInst = controller ? controller->GetDocument()->GetPresetCollection().PresetFromGUID(detailPresetID) : nullptr;
				if (detailPresetInst)
				{
					DetailName->ModumateTextBlock->SetText(detailPresetInst->DisplayName);
					bAllowEdit = true;
				}
				else
				{
					DetailName->ModumateTextBlock->SetText(LOCTEXT("EdgeDetail_Unknown", "Unknown"));
				}
			}
			else
			{
				DetailName->ModumateTextBlock->SetText(noDetailText);
			}
		}
		else if (numDetailPresets > 1)
		{
			DetailName->ModumateTextBlock->SetText(LOCTEXT("EdgeDetail_MixedValues", "Mixed Values"));
		}
	}
	else
	{
		DetailName->ModumateTextBlock->SetText(LOCTEXT("EdgeDetail_MixedConditions", "Mixed Conditions"));
	}

	ButtonSwap->SetVisibility(bAllowSwap ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonEdit->SetVisibility(bAllowEdit ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonDelete->SetVisibility(bAllowDelete ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UInstPropWidgetEdgeDetail::RegisterValue(UObject* Source, int32 EdgeID, const FGuid& DetailPresetID, uint32 DetailConditionHash)
{
	OnRegisteredValue(Source, CurrentConditionValue == DetailConditionHash);
	CurrentEdgeIDs.Add(EdgeID);
	CurrentPresetValues.Add(DetailPresetID);
	CurrentConditionValue = DetailConditionHash;
}

void UInstPropWidgetEdgeDetail::BroadcastValueChanged()
{
	ButtonPressedEvent.Broadcast(LastClickedAction);
	LastClickedAction = EEdgeDetailWidgetActions::None;
}

void UInstPropWidgetEdgeDetail::OnClickedSwap()
{
	LastClickedAction = EEdgeDetailWidgetActions::Swap;
	PostValueChanged();

	// TODO medium-term: open swap menu that can browse all edge detail assemblies that apply to the current selection of edges
	// TODO short-term: cycle through edge details that are valid for the given selection

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (!ensure(document && bConsistentValue && (CurrentEdgeIDs.Num() > 0) && (CurrentPresetValues.Num() == 1)))
	{
		return;
	}

	FGuid currentPresetID = *CurrentPresetValues.CreateConstIterator();
	auto& presetCollection = controller->GetDocument()->GetPresetCollection();

	// make an edge detail on the selected edges that have no current detail, from the current conditions and naive mitering
	if (!currentPresetID.IsValid())
	{
		bool bPopulatedDetailData = false;
		FEdgeDetailData newDetailData;
		TMap<int32, int32> detailOrientationsByEdgeID;
		TArray<int32> detailOrientations;

		// For each edge that this detail widget is assigning a new detail to, make sure the details all have matching conditions,
		// and figure out which orientation to apply for each one.
		for (int32 edgeID : CurrentEdgeIDs)
		{
			auto edgeMOI = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));
			if (!ensure(edgeMOI))
			{
				return;
			}

			if (bPopulatedDetailData)
			{
				FEdgeDetailData curDetailData(edgeMOI->GetMiterInterface());
				if (!ensure(newDetailData.CompareConditions(curDetailData, detailOrientations) && (detailOrientations.Num() > 0)))
				{
					return;
				}

				detailOrientationsByEdgeID.Add(edgeID, detailOrientations[0]);
			}
			else
			{
				newDetailData.FillFromMiterNode(edgeMOI->GetMiterInterface());
				detailOrientationsByEdgeID.Add(edgeID, 0);
			}
		}

		FBIMPresetInstance newDetailPreset;
		if ((presetCollection.GetBlankPresetForObjectType(EObjectType::OTEdgeDetail, newDetailPreset) == EBIMResult::Success) &&
			newDetailPreset.CustomData.SaveStructData(newDetailData))
		{
			// Try to make a unique display name for this detail preset
			TryMakeUniquePresetDisplayName(presetCollection, newDetailData, newDetailPreset.DisplayName);

			TArray<FDeltaPtr> deltas;

			auto makedetailPresetDelta = presetCollection.MakeCreateNewDelta(newDetailPreset);
			deltas.Add(makedetailPresetDelta);

			auto makeDetailMOIDelta = MakeShared<FMOIDelta>();
			for (int32 edgeID : CurrentEdgeIDs)
			{
				FMOIStateData newDetailState(document->GetNextAvailableID(), EObjectType::OTEdgeDetail, edgeID);
				newDetailState.AssemblyGUID = newDetailPreset.GUID;
				newDetailState.CustomData.SaveStructData(FMOIEdgeDetailData(detailOrientationsByEdgeID[edgeID]));

				makeDetailMOIDelta->AddCreateDestroyState(newDetailState, EMOIDeltaType::Create);
			}
			deltas.Add(makeDetailMOIDelta);

			document->ApplyDeltas(deltas, GetWorld());
		}
	}
	// cycle through the potential details for the selected condition
	else
	{
		FEdgeDetailData testDetailData;
		TArray<FGuid> otherValidDetailPresetIDs;
		EBIMResult searchResult = presetCollection.GetPresetsByPredicate([this, currentPresetID , &testDetailData](const FBIMPresetInstance& Preset) {
			return (Preset.GUID != currentPresetID) &&
				Preset.CustomData.LoadStructData(testDetailData, true) &&
				(testDetailData.CachedConditionHash == CurrentConditionValue); },
			otherValidDetailPresetIDs);
		if (searchResult != EBIMResult::Success)
		{
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("There are %d other edge detail presets with the same condition."), otherValidDetailPresetIDs.Num());
		// TODO: make a delta that cycles the selected edges to the next available edge detail 
	}
}

void UInstPropWidgetEdgeDetail::OnClickedEdit()
{
	LastClickedAction = EEdgeDetailWidgetActions::Edit;
	PostValueChanged();

	if (auto controller = GetOwningPlayer<AEditModelPlayerController>())
	{
		controller->EditModelUserWidget->SelectionTrayWidget->StartDetailDesignerFromSelection();
	}
}

void UInstPropWidgetEdgeDetail::OnClickedDelete()
{
	LastClickedAction = EEdgeDetailWidgetActions::Delete;
	PostValueChanged();

	// TODO: deletion/clearing of edge details should happen from within the swap menu and/or preset browsing, not the instance property menu

	// For now: clear the edge details of all selected edges, leaving the presets alone
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (document && (CurrentEdgeIDs.Num() > 0))
	{
		auto clearDetailsDelta = MakeShared<FMOIDelta>();
		for (int32 edgeID : CurrentEdgeIDs)
		{
			auto edgeMOI = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));
			if (!ensure(edgeMOI))
			{
				return;
			}

			clearDetailsDelta->AddCreateDestroyState(edgeMOI->CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
		}
		document->ApplyDeltas({ clearDetailsDelta }, GetWorld());
	}
}

bool UInstPropWidgetEdgeDetail::TryMakeUniquePresetDisplayName(const FBIMPresetCollection& PresetCollection, const FEdgeDetailData& NewDetailData, FText& OutDisplayName)
{
	TArray<FGuid> sameNameDetailPresetIDs;
	int32 displayNameIndex = 1;
	static const int32 maxDisplayNameIndex = 20;
	bool bFoundUnique = false;
	while (!bFoundUnique && displayNameIndex <= maxDisplayNameIndex)
	{
		OutDisplayName = NewDetailData.MakeShortDisplayText(displayNameIndex++);
		sameNameDetailPresetIDs.Reset();
		EBIMResult searchResult = PresetCollection.GetPresetsByPredicate([OutDisplayName](const FBIMPresetInstance& Preset) {
			return Preset.DisplayName.IdenticalTo(OutDisplayName, ETextIdenticalModeFlags::DeepCompare | ETextIdenticalModeFlags::LexicalCompareInvariants); },
			sameNameDetailPresetIDs);
		bFoundUnique = (searchResult != EBIMResult::Error) && (sameNameDetailPresetIDs.Num() == 0);
	}
	if (!bFoundUnique)
	{
		OutDisplayName = NewDetailData.MakeShortDisplayText();
	}

	return bFoundUnique;
}

#undef LOCTEXT_NAMESPACE
