// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Properties/InstPropWidgetEdgeDetail.h"

#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "BIMKernel/Presets/BIMPresetInstanceFactory.h"
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

	if (!ensure(DetailName && ButtonCreate && ButtonSwap && ButtonEdit))
	{
		return false;
	}

	ButtonCreate->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedCreate);
	ButtonSwap->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedSwap);
	ButtonEdit->ModumateButton->OnClicked.AddDynamic(this, &UInstPropWidgetEdgeDetail::OnClickedEdit);

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
	bool bSinglePresetID = (numDetailPresets == 1);
	FGuid detailPresetID = bSinglePresetID ? *CurrentPresetValues.CreateConstIterator() : FGuid();

	bool bAllowCreate = bConsistentValue && (CurrentEdgeIDs.Num() > 0) && bSinglePresetID && !detailPresetID.IsValid();
	bool bAllowSwap = bConsistentValue && (CurrentEdgeIDs.Num() > 0) && (numDetailPresets > 0);
	bool bAllowEdit = false;

	FText noDetailText = LOCTEXT("EdgeDetail_None", "None");

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (document)
	{
		auto* typicalDetail = document->TypicalEdgeDetails.Find(CurrentConditionValue);
		if (typicalDetail && *typicalDetail == detailPresetID)
		{
			PropertyTitle->ChangeText(FText(LOCTEXT("EdgeDetail_Typical","Detail (Typical)")));
		}
		else
		{
			PropertyTitle->ChangeText(FText(LOCTEXT("EdgeDetail_Typical", "Detail")));
		}
	}


	if (bConsistentValue)
	{
		if (numDetailPresets == 0)
		{
			DetailName->ChangeText(noDetailText);
		}
		else if (bSinglePresetID)
		{
			if (detailPresetID.IsValid())
			{
				FBIMPresetInstance* detailPresetInst = controller ? controller->GetDocument()->GetPresetCollection().PresetFromGUID(detailPresetID) : nullptr;
				if (detailPresetInst)
				{
					DetailName->ChangeText(detailPresetInst->DisplayName);
					bAllowEdit = true;
				}
				else
				{
					DetailName->ChangeText(LOCTEXT("EdgeDetail_Unknown", "Unknown"));
				}
			}
			else
			{
				DetailName->ChangeText(noDetailText);
			}
		}
		else if (numDetailPresets > 1)
		{
			DetailName->ChangeText(LOCTEXT("EdgeDetail_MixedValues", "Mixed Values"));
		}
	}
	else
	{
		DetailName->ChangeText(LOCTEXT("EdgeDetail_MixedConditions", "Mixed Conditions"));
	}

	ButtonCreate->SetVisibility(bAllowCreate ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonSwap->SetVisibility(bAllowSwap ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ButtonEdit->SetVisibility(bAllowEdit ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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

bool UInstPropWidgetEdgeDetail::OnCreateOrSwap(FGuid NewDetailPresetID)
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (!ensure(document && bConsistentValue && (CurrentEdgeIDs.Num() > 0) && (CurrentPresetValues.Num() == 1)))
	{
		return false;
	}

	bool bPopulatedDetailData = false;
	FEdgeDetailData newDetailData;
	FBIMPresetInstance* swapPresetInstance = nullptr;
	auto& presetCollection = document->GetPresetCollection();
	FGuid existingDetailPresetID = *CurrentPresetValues.CreateConstIterator();
	bool bClearingDetail = existingDetailPresetID.IsValid() && !NewDetailPresetID.IsValid();

	if (NewDetailPresetID.IsValid())
	{
		swapPresetInstance = presetCollection.PresetFromGUID(NewDetailPresetID);

		// If we passed in a new preset ID, it better already be in the preset collection and have detail data.
		if (!ensure(swapPresetInstance && swapPresetInstance->TryGetCustomData(newDetailData)))
		{
			return false;
		}

		bPopulatedDetailData = true;
	}

	// make an edge detail on the selected edges that have no current detail, from the current conditions and naive mitering
	TMap<int32, int32> validDetailOrientationByEdgeID;
	TArray<int32> tempDetailOrientations;

	// For each edge that this detail widget is assigning a new detail to, make sure the details all have matching conditions,
	// and figure out which orientation to apply for each one.
	if (!bClearingDetail)
	{
		for (int32 edgeID : CurrentEdgeIDs)
		{
			auto edgeMOI = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));
			if (!ensure(edgeMOI))
			{
				return false;
			}

			if (bPopulatedDetailData)
			{
				FEdgeDetailData curDetailData(edgeMOI->GetMiterInterface());
				if (!ensure(newDetailData.CompareConditions(curDetailData, tempDetailOrientations) && (tempDetailOrientations.Num() > 0)))
				{
					return false;
				}

				validDetailOrientationByEdgeID.Add(edgeID, tempDetailOrientations[0]);
			}
			else
			{
				newDetailData.FillFromMiterNode(edgeMOI->GetMiterInterface());
				validDetailOrientationByEdgeID.Add(edgeID, 0);
			}
		}
	}

	TArray<FDeltaPtr> deltas;

	// If we don't already have a new preset ID, and we're not clearing an existing detail, then make a delta to create one.
	if (!NewDetailPresetID.IsValid() && !bClearingDetail)
	{
		FBIMPresetInstance newDetailPreset;
		FBIMTagPath edgeDetalNCP = FBIMTagPath(TEXT("Property_Spatiality_ConnectionDetail_Edge"));
		if (!ensure((FBIMPresetInstanceFactory::CreateBlankPreset(edgeDetalNCP, presetCollection.PresetTaxonomy, newDetailPreset)) &&
			newDetailPreset.SetCustomData(newDetailData)==EBIMResult::Success))
		{
			return false;
		}

		// Try to make a unique display name for this detail preset
		TryMakeUniquePresetDisplayName(presetCollection, newDetailData, newDetailPreset.DisplayName);

		auto makedetailPresetDelta = presetCollection.MakeCreateNewDelta(newDetailPreset, this);
		NewDetailPresetID = newDetailPreset.GUID;
		deltas.Add(makedetailPresetDelta);
	}

	// Now, create MOI delta(s) for creating/updating/deleting the edge detail MOI(s) for the selected edge(s)
	auto updateDetailMOIDelta = MakeShared<FMOIDelta>();
	int nextID = document->GetNextAvailableID();
	for (int32 edgeID : CurrentEdgeIDs)
	{
		auto edgeMOI = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));

		// If there already was a preset and the new one is empty, then only delete the edge detail MOI for each selected edge.
		if (bClearingDetail && ensure(edgeMOI && edgeMOI->CachedEdgeDetailMOI))
		{
			updateDetailMOIDelta->AddCreateDestroyState(edgeMOI->CachedEdgeDetailMOI->GetStateData(), EMOIDeltaType::Destroy);
			continue;
		}

		// Otherwise, update the state data and detail assembly for the edge.
		FMOIStateData newDetailState;
		if (edgeMOI->CachedEdgeDetailMOI)
		{
			newDetailState = edgeMOI->CachedEdgeDetailMOI->GetStateData();
		}
		else
		{
			newDetailState = FMOIStateData(nextID++, EObjectType::OTEdgeDetail, edgeID);
		}

		newDetailState.AssemblyGUID = NewDetailPresetID;
		newDetailState.CustomData.SaveStructData(FMOIEdgeDetailData(validDetailOrientationByEdgeID[edgeID]));

		if (edgeMOI->CachedEdgeDetailMOI)
		{
			updateDetailMOIDelta->AddMutationState(edgeMOI->CachedEdgeDetailMOI, edgeMOI->CachedEdgeDetailMOI->GetStateData(), newDetailState);
		}
		else
		{
			updateDetailMOIDelta->AddCreateDestroyState(newDetailState, EMOIDeltaType::Create);
		}
	}

	if (updateDetailMOIDelta->IsValid())
	{
		deltas.Add(updateDetailMOIDelta);
	}

	return document->ApplyDeltas(deltas, GetWorld());
}

void UInstPropWidgetEdgeDetail::OnClickedCreate()
{
	LastClickedAction = EEdgeDetailWidgetActions::Create;
	PostValueChanged();

	OnCreateOrSwap(FGuid());
}

void UInstPropWidgetEdgeDetail::OnClickedSwap()
{
	LastClickedAction = EEdgeDetailWidgetActions::Swap;
	PostValueChanged();

	// TODO: open swap menu that can browse all edge detail assemblies that apply to the current selection of edges

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (!ensure(document))
	{
		return;
	}

	// Start with an empty preset ID at the beginning of the list, so swapping to the first element should clear it.
	TArray<FGuid> validDetailPresetIDs = { FGuid() };
	int32 newPresetIdx = 0;

	// If we're selecting a single detail preset, then get a list of all the detail presets that could be applied to the same condition.
	if ((bConsistentValue && (CurrentPresetValues.Num() == 1)))
	{
		FGuid currentPresetID = *CurrentPresetValues.CreateConstIterator();
		auto& presetCollection = document->GetPresetCollection();

		// cycle through edge details that are valid for the given selection
		FEdgeDetailData testDetailData;
		EBIMResult searchResult = presetCollection.GetPresetsByPredicate([this, &testDetailData](const FBIMPresetInstance& Preset) {
			return Preset.TryGetCustomData(testDetailData) &&
				(testDetailData.CachedConditionHash == CurrentConditionValue); },
			validDetailPresetIDs);

		int32 curPresetIdx = validDetailPresetIDs.Find(currentPresetID);
		if (!ensure((searchResult == EBIMResult::Success) && (curPresetIdx != INDEX_NONE)))
		{
			return;
		}

		newPresetIdx = (curPresetIdx + 1) % validDetailPresetIDs.Num();
	}

	if (validDetailPresetIDs.IsValidIndex(newPresetIdx))
	{
		FGuid nextDetailPresetID = validDetailPresetIDs[newPresetIdx];
		OnCreateOrSwap(nextDetailPresetID);
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
