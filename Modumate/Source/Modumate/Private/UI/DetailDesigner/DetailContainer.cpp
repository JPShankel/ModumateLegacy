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

bool UDetailDesignerContainer::BuildEditor(const FGuid& InDetailPresetID, const TSet<int32>& InEdgeIDs)
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
	auto playerHUD = controller ? controller->GetEditModelHUD() : nullptr;
	if (!ensure(document && playerHUD))
	{
		return false;
	}

	auto& presetCollection = document->GetPresetCollection();

	FEdgeDetailData detailData;
	auto detailPreset = presetCollection.PresetFromGUID(DetailPresetID);
	if (!ensure(detailPreset && detailPreset->TryGetCustomData(detailData)))
	{
		return false;
	}

	int32 numParticipants = detailData.Conditions.Num();
	if (!ensure((numParticipants == detailData.Overrides.Num()) && (numParticipants > 0)))
	{
		return false;
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
				return false;
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

		EDetailParticipantType participantDetailType = detailCondition.Type;
		FBIMAssemblySpec* participantAssembly = (OrientationIdx != INDEX_NONE) ? &participantAssemblies[participantIdx] : nullptr;
		EObjectType participantObjectType = participantAssembly ? participantAssembly->ObjectType : EObjectType::OTNone;
		int32& objectTypeIndex = participantsByType.FindOrAdd(participantObjectType, 0);
		objectTypeIndex++;

		// If we're referencing a real assembly, double-check that the condition layers match the desired participant type.
		int32 numDetailLayers = detailCondition.LayerThicknesses.Num();
		int32 numAssemblyLayers = participantAssembly ? participantAssembly->Layers.Num() : numDetailLayers;
		switch (participantDetailType)
		{
		case EDetailParticipantType::Layered:
			if (participantAssembly && !ensure(numDetailLayers == numAssemblyLayers))
			{
				return false;
			}
			break;
		case EDetailParticipantType::Rigged:
		case EDetailParticipantType::Extruded:
			if ((numDetailLayers != 1) || (participantAssembly && !ensure(numAssemblyLayers == 0)))
			{
				return false;
			}
			break;
		case EDetailParticipantType::None:
		default:
			return false;
		}

		// Create a header for the participant, with its participant number, thickness, and optional assembly name
		auto presetTitle = GetOrCreateParticipantWidget<UDetailDesignerAssemblyTitle>(participantWidgetIdx++, playerHUD, ParticipantTitleClass);

		const auto& settings = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController())->GetDocument()->GetCurrentSettings();

		FText numberPrefix = participantAssembly ? UModumateTypeStatics::GetTextForObjectType(participantObjectType) : LOCTEXT("AssemblyAnonPrefix", "Participant");
		float totalThicknessInches = Algo::Accumulate(detailCondition.LayerThicknesses, 0.0f);
		FText totalThicknessText = UModumateDimensionStatics::InchesToDisplayText(totalThicknessInches,1,settings.DimensionType,settings.DimensionUnit);
		FText assemblyNameText = participantAssembly ? FText::Format(LOCTEXT("AssemblyNameFormat", ", {0}"), FText::FromString(participantAssembly->DisplayName)) : FText::GetEmpty();

		FText assemblyTitle = FText::Format(LOCTEXT("AssemblyTitleFormat", "{0} {1}, ({2}){3}"),
			numberPrefix, objectTypeIndex, totalThicknessText, assemblyNameText);
		presetTitle->AssemblyTitle->ChangeText(assemblyTitle);

		// Create a header for layered rows
		if (participantDetailType == EDetailParticipantType::Layered)
		{
			GetOrCreateParticipantWidget(participantWidgetIdx++, playerHUD, ParticipantColumnTitlesClass);
		}

		// Create a row for each layer of the participant, as well as two extras for the SurfaceExtensions front and back values.
		// For the purposes of indexing layers in conditions/details, the layer index of surface extensions is numDetailLayers + 1,
		// but the front and back values are displayed and edited separately for convenience.
		int32 numDetailEntries = (participantDetailType == EDetailParticipantType::Layered) ? numDetailLayers + 2 : 1;
		for (int32 detailEntryIdx = 0; detailEntryIdx < numDetailEntries; ++detailEntryIdx)
		{
			auto presetLayerData = GetOrCreateParticipantWidget<UDetailDesignerLayerData>(participantWidgetIdx++, playerHUD, ParticipantLayerDataClass);
			presetLayerData->OnExtensionChanged.Clear();
			presetLayerData->OnExtensionChanged.AddDynamic(this, &UDetailDesignerContainer::OnLayerExtensionChanged);

			FText layerTitle = FText::GetEmpty();
			FVector2D extensionValues(ForceInitToZero);
			bool extensionsEnabled[2] = { true, true };

			if (participantDetailType == EDetailParticipantType::Layered)
			{
				if ((detailEntryIdx == 0) || (detailEntryIdx == (numDetailEntries - 1)))
				{
					layerTitle = LOCTEXT("SurfaceTitle", "Surface Extension");
					presetLayerData->LayerThickness->SetVisibility(ESlateVisibility::Collapsed);
					extensionValues = detailOverride.SurfaceExtensions;
					int32 extensionIndexToDisable = (detailEntryIdx == 0) ? 1 : 0;
					extensionsEnabled[extensionIndexToDisable] = false;
				}
				else
				{
					int32 layerIndex = detailEntryIdx - 1;
					float layerThicknessInches = detailCondition.LayerThicknesses[layerIndex];

					layerTitle = FText::Format(LOCTEXT("LayerTitle", "Layer {0}"), layerIndex + 1);
					if (participantAssembly)
					{
						if (!ensure(participantAssembly->Layers.IsValidIndex(layerIndex)))
						{
							return false;
						}

						// Double-check that if we're referencing a real assembly, that it has the right layers
						auto& participantLayer = participantAssembly->Layers[layerIndex];
						auto layerPreset = presetCollection.PresetFromGUID(participantLayer.PresetGUID);
						if (!ensure(layerPreset &&
							FMath::IsNearlyEqual(layerThicknessInches, UModumateDimensionStatics::CentimetersToInches64(participantLayer.ThicknessCentimeters))))
						{
							return false;
						}

						layerTitle = layerPreset->DisplayName;
					}
					presetLayerData->LayerThickness->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					presetLayerData->LayerThickness->ChangeText(UModumateDimensionStatics::InchesToDisplayText(layerThicknessInches,1,settings.DimensionType,settings.DimensionUnit));
					extensionValues = detailOverride.LayerExtensions[layerIndex];
				}
			}
			else
			{
				layerTitle = LOCTEXT("NonLayerTitle", "Extension:");
				presetLayerData->LayerThickness->SetVisibility(ESlateVisibility::Collapsed);
				extensionValues = detailOverride.LayerExtensions[detailEntryIdx];
				extensionsEnabled[1] = false;
			}

			presetLayerData->LayerName->ChangeText(layerTitle);
			presetLayerData->PopulateLayerData(participantIdx, detailEntryIdx, extensionValues, extensionsEnabled[0], extensionsEnabled[1]);
		}

		if (participantIdx < (numParticipants - 1))
		{
			GetOrCreateParticipantWidget(participantWidgetIdx++, playerHUD, ParticipantSeparatorLineClass);
		}
	}

	ClearParticipantEntries(participantWidgetIdx);

	return true;
}

void UDetailDesignerContainer::ClearParticipantEntries(int32 StartIndex)
{
	int32 numEntries = ParticipantsList->GetChildrenCount();
	for (int32 entryIdx = numEntries - 1; entryIdx >= StartIndex; --entryIdx)
	{
		ParticipantsList->RemoveChildAt(entryIdx);
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

void UDetailDesignerContainer::OnLayerExtensionChanged(int32 ParticipantIndex, int32 DetailEntryIdx, FVector2D NewExtensions)
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
	if (!ensure(presetDelta && presetDelta->NewState.TryGetCustomData(detailData) &&
		detailData.Overrides.IsValidIndex(ParticipantIndex)))
	{
		return;
	}

	const auto& condition = detailData.Conditions[ParticipantIndex];
	auto& overrideParticipant = detailData.Overrides[ParticipantIndex];
	EDetailParticipantType participantType = condition.Type;
	int32 layerIndex = INDEX_NONE;
	int32 numLayers = overrideParticipant.LayerExtensions.Num();

	if (participantType == EDetailParticipantType::Layered)
	{
		int32 numEntries = numLayers + 2;
		if ((DetailEntryIdx == 0) || (DetailEntryIdx == (numEntries - 1)))
		{
			int32 surfaceExtensionIdx = (DetailEntryIdx == 0) ? 0 : 1;
			overrideParticipant.SurfaceExtensions[surfaceExtensionIdx] = NewExtensions[surfaceExtensionIdx];
		}
		else
		{
			layerIndex = DetailEntryIdx - 1;
		}
	}
	else
	{
		// For non-layered extensions, both values must be equal, but only X values come in, so set Y to X for consistency.
		layerIndex = DetailEntryIdx;
		NewExtensions.Y = NewExtensions.X;
	}

	if (layerIndex != INDEX_NONE)
	{
		overrideParticipant.LayerExtensions[layerIndex] = NewExtensions;
	}

	if (!ensure(presetDelta->NewState.SetCustomData(detailData) == EBIMResult::Success))
	{
		return;
	}

	document->ApplyDeltas({ presetDelta }, controller->GetWorld());
}

#undef LOCTEXT_NAMESPACE
