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
	PresetName->EditText->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UDetailDesignerContainer::OnPresetNameEdited);

	return true;
}

void UDetailDesignerContainer::ClearEditor()
{
	ParticipantsList->ClearChildren();
	DetailPresetID.Invalidate();
	EdgeIDs.Reset();
	OrientationIdx = INDEX_NONE;
}

void UDetailDesignerContainer::BuildEditor(const FGuid& InDetailPresetID, const TSet<int32>& InEdgeIDs)
{
	int32 numEdgeIDs = InEdgeIDs.Num();
	if ((DetailPresetID != InDetailPresetID) || (InEdgeIDs.Intersect(EdgeIDs).Num() != numEdgeIDs))
	{
		ClearEditor();
	}

	DetailPresetID = InDetailPresetID;
	EdgeIDs = InEdgeIDs;

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
	int32 edgeID = (EdgeIDs.Num() == 1) ? *EdgeIDs.CreateConstIterator() : MOD_ID_NONE;
	auto edgeObj = Cast<AMOIMetaEdge>(document->GetObjectById(edgeID));
	OrientationIdx = (edgeObj && edgeObj->CachedEdgeDetailMOI) ? edgeObj->CachedEdgeDetailMOI->InstanceData.OrientationIndex : INDEX_NONE;
	if (OrientationIdx != INDEX_NONE)
	{
		bool bOrientationIsReversed = (OrientationIdx >= numParticipants);
		int32 rotationIdx = OrientationIdx % numParticipants;

		auto& miterData = edgeObj->GetMiterData();
		for (int32 participantID : miterData.SortedMiterIDs)
		{
			auto participantObj = document->GetObjectById(participantID);
			if (!ensure(participantObj))
			{
				ClearEditor();
				return;
			}

			auto& miterParticipant = miterData.ParticipantsByID[participantID];
			auto participantAssembly = participantObj->GetAssembly();
			if (!miterParticipant.bPlaneNormalCW ^ bOrientationIsReversed)
			{
				participantAssembly.ReverseLayers();
			}

			participantAssemblies.Add(participantAssembly);
		}

		if (bOrientationIsReversed)
		{
			Algo::Reverse(participantAssemblies);
		}
		else
		{
			// Since we are orienting the edge's data to match the preset, rather than the preset to match the edge (which is the case when applying a detail),
			// we may need to rotate in the opposite direction. A reversed orientation cancels this out, so for example:
			// A 4-participant edge detail preset 1234 with orientation 1 to match edge 2341 must then rotate the edge's miter participants 3 times to match the preset.
			// The same preset 1234 with orientation 5 to match to match edge 3214 can still rotate the edge's miter participants 5 times to match the preset.
			rotationIdx = (numParticipants - rotationIdx) % numParticipants;
		}

		for (int32 i = 0; i < rotationIdx; ++i)
		{
			auto firstAssembly = participantAssemblies[0];
			participantAssemblies.Add(firstAssembly);
			participantAssemblies.RemoveAt(0, 1, false);
		}
	}

	int32 participantWidgetIdx = 0;
	TMap<EObjectType, int32> participantsByType;
	for (int32 participantIdx = 0; participantIdx < numParticipants; ++participantIdx)
	{
		const auto& detailCondition = detailData.Conditions[participantIdx];
		const auto& detailOverride = detailData.Overrides[participantIdx];

		FBIMAssemblySpec* participantAssembly = (OrientationIdx != INDEX_NONE) ? &participantAssemblies[participantIdx] : nullptr;
		EObjectType participantType = participantAssembly ? participantAssembly->ObjectType : EObjectType::OTNone;
		int32& typeIndex = participantsByType.FindOrAdd(participantType, 0);
		typeIndex++;

		// If we're referencing a real assembly, double-check that it matches the reported number of layers
		int32 numLayers = detailCondition.LayerThicknesses.Num();
		if (participantAssembly && !ensure(numLayers == participantAssembly->Layers.Num()))
		{
			ClearEditor();
			return;
		}

		// Create a header for the participant, with its participant number, thickness, and optional assembly name
		auto presetTitle = Cast<UDetailDesignerAssemblyTitle>(ParticipantsList->GetChildAt(participantWidgetIdx++));
		if (presetTitle == nullptr)
		{
			presetTitle = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UDetailDesignerAssemblyTitle>(ParticipantTitleClass);
			ParticipantsList->AddChildToVerticalBox(presetTitle);
		}

		FText numberPrefix = participantAssembly ? UModumateTypeStatics::GetTextForObjectType(participantType) : LOCTEXT("AssemblyAnonPrefix", "Participant");
		float totalThicknessInches = Algo::Accumulate(detailCondition.LayerThicknesses, 0.0f);
		FText totalThicknessText = UModumateDimensionStatics::InchesToImperialText(totalThicknessInches);
		FText assemblyNameText = participantAssembly ? FText::Format(LOCTEXT("AssemblyNameFormat", ", {0}"), FText::FromString(participantAssembly->DisplayName)) : FText::GetEmpty();

		FText assemblyTitle = FText::Format(LOCTEXT("AssemblyTitleFormat", "{0} {1}, ({2}){3}"),
			numberPrefix, typeIndex, totalThicknessText, assemblyNameText);
		presetTitle->AssemblyTitle->ChangeText(assemblyTitle);

		// Create a header for the layer rows
		auto presetColumnTitles = ParticipantsList->GetChildAt(participantWidgetIdx++);
		if ((presetColumnTitles == nullptr) || !presetColumnTitles->IsA(ParticipantColumnTitlesClass.Get()))
		{
			presetColumnTitles = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UUserWidget>(ParticipantColumnTitlesClass);
			ParticipantsList->AddChildToVerticalBox(presetColumnTitles);
		}

		// Create a row for each layer of the participant, as well as an extra one for the SurfaceExtensions values.
		// TODO: If we clean up the UI to use two rows for the SurfaceExtensions (one for start, one for end), then this would need to create two additional UI entries.
		for (int32 layerIdx = 0; layerIdx <= numLayers; ++layerIdx)
		{
			auto presetLayerData = Cast<UDetailDesignerLayerData>(ParticipantsList->GetChildAt(participantWidgetIdx++));
			if (presetLayerData == nullptr)
			{
				presetLayerData = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UDetailDesignerLayerData>(ParticipantLayerDataClass);
				presetLayerData->OnExtensionChanged.AddDynamic(this, &UDetailDesignerContainer::OnLayerExtensionChanged);
				ParticipantsList->AddChildToVerticalBox(presetLayerData);
			}

			if (layerIdx == numLayers)
			{
				presetLayerData->LayerName->ChangeText(LOCTEXT("SurfaceTitle", "Surface Extensions"));
				presetLayerData->LayerThickness->SetVisibility(ESlateVisibility::Collapsed);
				presetLayerData->PopulateLayerData(participantIdx, layerIdx, detailOverride.SurfaceExtensions);
			}
			else
			{
				float layerThicknessInches = detailCondition.LayerThicknesses[layerIdx];

				FText layerTitle = FText::Format(LOCTEXT("LayerTitle", "Layer {0}"), layerIdx + 1);
				if (participantAssembly)
				{
					// Double-check that if we're referencing a real assembly, that it has the right layers
					auto& participantLayer = participantAssembly->Layers[layerIdx];
					auto layerPreset = presetCollection.PresetFromGUID(participantLayer.PresetGUID);
					if (!ensure(layerPreset &&
						FMath::IsNearlyEqual(layerThicknessInches, UModumateDimensionStatics::CentimetersToInches64(participantLayer.ThicknessCentimeters))))
					{
						ClearEditor();
						return;
					}

					layerTitle = layerPreset->DisplayName;
				}

				presetLayerData->LayerName->ChangeText(layerTitle);
				presetLayerData->LayerThickness->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				presetLayerData->LayerThickness->ChangeText(UModumateDimensionStatics::InchesToImperialText(layerThicknessInches));
				presetLayerData->PopulateLayerData(participantIdx, layerIdx, detailOverride.LayerExtensions[layerIdx]);
			}
		}

		if (participantIdx < (numParticipants - 1))
		{
			auto participantSeparator = ParticipantsList->GetChildAt(participantWidgetIdx++);
			if ((participantSeparator == nullptr) || !participantSeparator->IsA(ParticipantSeparatorLineClass.Get()))
			{
				ParticipantsList->AddChildToVerticalBox(controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UUserWidget>(ParticipantSeparatorLineClass));
			}
		}
	}
}

void UDetailDesignerContainer::OnPressedCancel()
{
	if (auto controller = GetOwningPlayer<AEditModelPlayerController>())
	{
		controller->EditModelUserWidget->SelectionTrayWidget->OpenToolTrayForSelection();
	}
}

void UDetailDesignerContainer::OnPresetNameEdited(const FText& Text, ETextCommit::Type CommitMethod)
{
	auto controller = GetOwningPlayer<AEditModelPlayerController>();
	auto document = controller ? controller->GetDocument() : nullptr;
	if (!ensure(document) || (CommitMethod == ETextCommit::OnCleared))
	{
		return;
	}

	// Make a delta for modifying the display name of the current preset
	auto presetDelta = document->GetPresetCollection().MakeUpdateDelta(DetailPresetID, this);
	if (!ensure(presetDelta))
	{
		return;
	}

	presetDelta->NewState.DisplayName = Text;
	document->ApplyDeltas({ presetDelta }, controller->GetWorld());
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

	// Make a delta for modifying the extensions of the current preset,
	// and make sure it can support the incoming extension values.
	// TODO: cache the detail data instead if it's safe
	FEdgeDetailData detailData;
	auto presetDelta = document->GetPresetCollection().MakeUpdateDelta(DetailPresetID, this);
	if (!ensure(presetDelta && presetDelta->NewState.CustomData.LoadStructData(detailData) &&
		detailData.Overrides.IsValidIndex(ParticipantIndex)))
	{
		return;
	}

	auto& overrideParticipant = detailData.Overrides[ParticipantIndex];

	int32 numLayers = overrideParticipant.LayerExtensions.Num();
	if (!ensure((LayerIndex >= 0) && (LayerIndex <= numLayers)))
	{
		return;
	}

	
	if (LayerIndex < numLayers)
	{
		overrideParticipant.LayerExtensions[LayerIndex] = NewExtensions;
	}
	else
	{
		overrideParticipant.SurfaceExtensions = NewExtensions;
	}

	if (!ensure(presetDelta->NewState.CustomData.SaveStructData(detailData)))
	{
		return;
	}

	document->ApplyDeltas({ presetDelta }, controller->GetWorld());
}

#undef LOCTEXT_NAMESPACE
