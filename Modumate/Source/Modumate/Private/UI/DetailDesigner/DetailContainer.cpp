// Copyright 2021 Modumate, Inc. All Rights Reserved

#include "UI/DetailDesigner/DetailContainer.h"

#include "Algo/Accumulate.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "Components/VerticalBox.h"
#include "Database/ModumateObjectEnums.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Objects/EdgeDetailObj.h"
#include "Objects/MetaEdge.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/DetailDesigner/DetailAssemblyTitle.h"
#include "UI/DetailDesigner/DetailLayerData.h"
#include "UI/DetailDesigner/DetailPresetName.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/EditModelUserWidget.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"

#define LOCTEXT_NAMESPACE "ModumateDetailDesigner"

bool UDetailDesignerContainer::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ensure(MainTitle && SubTitle && ButtonCancel && PresetName && ParticipantsList &&
		ParticipantTitleClass && ParticipantColumnTitlesClass && ParticipantLayerDataClass && ParticipantSeparatorLineClass))
	{
		return false;
	}

	ButtonCancel->ModumateButton->OnClicked.AddDynamic(this, &UDetailDesignerContainer::OnPressedCancel);

	return true;
}

void UDetailDesignerContainer::BuildEditor(const FGuid& InDetailPresetID, const TSet<int32>& InEdgeIDs)
{
	DetailPresetID = InDetailPresetID;
	EdgeIDs = InEdgeIDs;

	ParticipantsList->ClearChildren();

	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (!ensure(document))
	{
		return;
	}

	auto& presetCollection = document->GetPresetCollection();

	FEdgeDetailData detailData;
	auto detailPreset = presetCollection.PresetFromGUID(DetailPresetID);
	if (!ensure(detailPreset && detailPreset->CustomData.LoadStructData(detailData)))
	{
		return;
	}

	int32 numParticipants = detailData.Conditions.Num();
	if (!ensure((numParticipants == detailData.Overrides.Num()) && (numParticipants > 0)))
	{
		return;
	}

	PresetName->EditText->ModumateEditableTextBox->SetText(detailPreset->DisplayName);

	// Find the miter participants for the edge(s) that we're editing.
	// TODO: determine if we can display participant data if our selection all has the same detail orientation;
	// for now, only show it if 1 edge is selected.
	TArray<FBIMAssemblySpec> participantAssemblies;
	ParticipantsReversed.Reset();
	int32 edgeID = (EdgeIDs.Num() == 1) ? *EdgeIDs.CreateConstIterator() : MOD_ID_NONE;
	auto edgeObj = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));
	OrientationIdx = (edgeObj && edgeObj->CachedEdgeDetailMOI) ? edgeObj->CachedEdgeDetailMOI->InstanceData.OrientationIndex : INDEX_NONE;
	bool bOrientationIsReversed = (OrientationIdx >= numParticipants);
	if (OrientationIdx != INDEX_NONE)
	{
		// TODO: shuffle participants based on orientation, don't just flip
		if ((OrientationIdx % numParticipants) != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Detail \"%s\" (%s) is applied to edge #%d with orientation index %d"),
				*detailPreset->DisplayName.ToString(), *DetailPresetID.ToString(EGuidFormats::Short), edgeID, OrientationIdx);
		}

		auto& miterData = edgeObj->GetMiterData();
		for (int32 participantID : miterData.SortedMiterIDs)
		{
			auto participantObj = document->GetObjectById(participantID);
			if (!ensure(participantObj))
			{
				return;
			}

			participantAssemblies.Add(participantObj->GetAssembly());
			auto& miterParticipant = miterData.ParticipantsByID[participantID];
			ParticipantsReversed.Add(!miterParticipant.bPlaneNormalCW);
		}

		if (bOrientationIsReversed)
		{
			Algo::Reverse(participantAssemblies);
		}
	}

	TMap<EObjectType, int32> participantsByType;
	for (int32 participantIdx = 0; participantIdx < numParticipants; ++participantIdx)
	{
		const auto& detailCondition = detailData.Conditions[participantIdx];
		const auto& detailOverride = detailData.Overrides[participantIdx];

		FBIMAssemblySpec* participantAssembly = (OrientationIdx != INDEX_NONE) ? &participantAssemblies[participantIdx] : nullptr;
		EObjectType participantType = participantAssembly ? participantAssembly->ObjectType : EObjectType::OTNone;
		int32& typeIndex = participantsByType.FindOrAdd(participantType, 0);
		bool bParticipantReversed = participantAssembly && ParticipantsReversed[participantIdx];
		typeIndex++;

		// If we're referencing a real assembly, double-check that it matches the reported number of layers
		int32 numLayers = detailCondition.LayerThicknesses.Num();
		if (participantAssembly && !ensure(numLayers == participantAssembly->Layers.Num()))
		{
			return;
		}

		// Create a header for the participant, with its participant number, thickness, and optional assembly name
		auto presetTitle = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UDetailDesignerAssemblyTitle>(ParticipantTitleClass);
		ParticipantsList->AddChildToVerticalBox(presetTitle);

		FText numberPrefix = participantAssembly ? UModumateTypeStatics::GetTextForObjectType(participantType) : LOCTEXT("AssemblyAnonPrefix", "Participant");
		float totalThicknessInches = Algo::Accumulate(detailCondition.LayerThicknesses, 0.0f);
		FText totalThicknessText = UModumateDimensionStatics::InchesToImperialText(totalThicknessInches);
		FText assemblyNameText = participantAssembly ? FText::Format(LOCTEXT("AssemblyNameFormat", ", {0}"), FText::FromString(participantAssembly->DisplayName)) : FText::GetEmpty();

		FText assemblyTitle = FText::Format(LOCTEXT("AssemblyTitleFormat", "{0} {1}, ({2}){3}"),
			numberPrefix, participantIdx + 1, totalThicknessText, assemblyNameText);
		presetTitle->AssemblyTitle->ModumateTextBlock->SetText(assemblyTitle);

		// Create a header for the layer rows
		auto presetColumnTitles = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UUserWidget>(ParticipantColumnTitlesClass);
		ParticipantsList->AddChildToVerticalBox(presetColumnTitles);

		// Create a row for each layer of the participant
		for (int32 layerIdx = 0; layerIdx < numLayers; ++layerIdx)
		{
			auto presetLayerData = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UDetailDesignerLayerData>(ParticipantLayerDataClass);
			ParticipantsList->AddChildToVerticalBox(presetLayerData);

			float layerThicknessInches = detailCondition.LayerThicknesses[layerIdx];

			FText layerTitle = FText::Format(LOCTEXT("LayerTitle", "Layer {0}"), layerIdx + 1);
			if (participantAssembly)
			{
				// Double-check that if we're referencing a real assembly, that it has the right layers
				int32 assemblyLayerIdx = (bOrientationIsReversed ^ bParticipantReversed) ? (numLayers - 1 - layerIdx) : layerIdx;
				auto& participantLayer = participantAssembly->Layers[assemblyLayerIdx];
				auto layerPreset = presetCollection.PresetFromGUID(participantLayer.PresetGUID);
				if (!ensure(layerPreset &&
					FMath::IsNearlyEqual(layerThicknessInches, UModumateDimensionStatics::CentimetersToInches64(participantLayer.ThicknessCentimeters))))
				{
					return;
				}

				layerTitle = layerPreset->DisplayName;
			}

			presetLayerData->LayerName->ModumateTextBlock->SetText(layerTitle);

			presetLayerData->LayerThickness->ModumateTextBlock->SetText(UModumateDimensionStatics::InchesToImperialText(layerThicknessInches));

			FVector2D extensions = detailOverride.LayerExtensions[layerIdx];
			if (bOrientationIsReversed)
			{
				Swap(extensions.X, extensions.Y);
			}

			presetLayerData->ExtensionFront->ModumateEditableTextBox->SetText(UModumateDimensionStatics::CentimetersToImperialText(extensions.X));
			presetLayerData->ExtensionBack->ModumateEditableTextBox->SetText(UModumateDimensionStatics::CentimetersToImperialText(extensions.Y));

			presetLayerData->PopulateLayerData(participantIdx, layerIdx, extensions);
			presetLayerData->OnExtensionChanged.AddDynamic(this, &UDetailDesignerContainer::OnLayerExtensionChanged);
		}

		ParticipantsList->AddChildToVerticalBox(controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UUserWidget>(ParticipantSeparatorLineClass));
	}
}

void UDetailDesignerContainer::OnPressedCancel()
{
	if (auto controller = GetOwningPlayer<AEditModelPlayerController>())
	{
		controller->EditModelUserWidget->SelectionTrayWidget->OpenToolTrayForSelection();
	}
}

void UDetailDesignerContainer::OnLayerExtensionChanged(int32 ParticipantIndex, int32 LayerIndex, FVector2D NewExtensions)
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (!ensure(document))
	{
		return;
	}

	auto& presetCollection = document->GetPresetCollection();

	// Reload the detail from our current preset, and make sure it can support the incoming extension values
	// TODO: cache the detail data instead if it's safe
	FEdgeDetailData detailData;
	auto detailPreset = presetCollection.PresetFromGUID(DetailPresetID);
	if (!ensure(detailPreset && detailPreset->CustomData.LoadStructData(detailData) &&
		detailData.Overrides.IsValidIndex(ParticipantIndex) &&
		detailData.Overrides[ParticipantIndex].LayerExtensions.IsValidIndex(LayerIndex)))
	{
		return;
	}

	auto detailPresetDelta = MakeShared<FBIMPresetDelta>();
	detailPresetDelta->OldState = *detailPreset;
	detailPresetDelta->NewState = *detailPreset;

	if (ParticipantsReversed.IsValidIndex(ParticipantIndex) && ParticipantsReversed[ParticipantIndex])
	{
		Swap(NewExtensions.X, NewExtensions.Y);
	}

	detailData.Overrides[ParticipantIndex].LayerExtensions[LayerIndex] = NewExtensions;
	if (!ensure(detailPresetDelta->NewState.CustomData.SaveStructData(detailData)))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Detail \"%s\" (%s) participant #%d layer #%d new extensions: [%.2f, %.2f]"),
		*detailPreset->DisplayName.ToString(), *DetailPresetID.ToString(EGuidFormats::Short), ParticipantIndex, LayerIndex, NewExtensions.X, NewExtensions.Y);

	document->ApplyDeltas({ detailPresetDelta }, controller->GetWorld());
}

#undef LOCTEXT_NAMESPACE
