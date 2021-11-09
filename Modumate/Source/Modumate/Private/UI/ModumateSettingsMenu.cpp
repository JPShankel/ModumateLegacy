// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ModumateSettingsMenu.h"

#include "Components/ComboBoxString.h"
#include "DocumentManagement/DocumentSettings.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/EnumHelpers.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Online/ModumateVoice.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateComboBoxString.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateSlider.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/SkyActor.h"

bool UModumateSettingsMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(DimensionPrefDropdown && DistIncrementDropdown && CloseButton && MicSourceDropdown && SpeakerSourceDropdown && 
		SliderGraphicShadows && SliderGraphicAntiAliasing && ButtonAutoDetectGraphicsSettings))
	{
		return false;
	}

	DimensionPrefDropdown->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UModumateSettingsMenu::OnDimensionPrefChanged);
	DistIncrementDropdown->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UModumateSettingsMenu::OnDistIncrementPrefChanged);
	CloseButton->ModumateButton->OnClicked.AddDynamic(this, &UModumateSettingsMenu::OnCloseButtonClicked);
	TextEditableLatitude->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UModumateSettingsMenu::OnTextEditableLatitudeCommitted);
	TextEditableLongitude->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UModumateSettingsMenu::OnTextEditableLongitudeCommitted);
	TextEditableNorth->ModumateEditableTextBox->OnTextCommitted.AddDynamic(this, &UModumateSettingsMenu::OnTextEditableNorthCommitted);
	MicSourceDropdown->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UModumateSettingsMenu::OnMicDeviceChanged);
	SpeakerSourceDropdown->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UModumateSettingsMenu::OnSpeakerDeviceChanged);
	ButtonAutoDetectGraphicsSettings->ModumateButton->OnReleased.AddDynamic(this, &UModumateSettingsMenu::OnButtonAutoDetectSettingsReleased);
	SliderGraphicShadows->OnMouseCaptureEnd.AddDynamic(this, &UModumateSettingsMenu::OnMouseCaptureEndSliderGraphicShadows);
	SliderGraphicAntiAliasing->OnMouseCaptureEnd.AddDynamic(this, &UModumateSettingsMenu::OnMouseCaptureEndSliderGraphicAntiAliasing);

	UModumateComboBoxString* cbox_mic = Cast<UModumateComboBoxString>(MicSourceDropdown->ComboBoxStringJustification);
	UModumateComboBoxString* cbox_spk = Cast<UModumateComboBoxString>(SpeakerSourceDropdown->ComboBoxStringJustification);

	if (cbox_mic && cbox_spk)
	{
		cbox_mic->ItemWidgetClass = ItemWidgetOverrideClass;
		cbox_spk->ItemWidgetClass = ItemWidgetOverrideClass;
	}

	SpeakerSourceDropdown->ComboBoxStringJustification->ClearOptions();
	MicSourceDropdown->ComboBoxStringJustification->ClearOptions();

	// Populate dimension preference dropdown
	DimPrefsByDisplayString.Reset();
	DimensionPrefDropdown->ComboBoxStringJustification->ClearOptions();
	auto dimensionPrefEnum = StaticEnum<EDimensionPreference>();
	int32 numEnums = dimensionPrefEnum->NumEnums();
	if (dimensionPrefEnum->ContainsExistingMax())
	{
		numEnums--;
	}

	for (int32 dimensionPrefIdx = 0; dimensionPrefIdx < numEnums; ++dimensionPrefIdx)
	{
		EDimensionPreference dimensionPrefValue = static_cast<EDimensionPreference>(dimensionPrefEnum->GetValueByIndex(dimensionPrefIdx));
		FText dimensionPrefDisplayName = dimensionPrefEnum->GetDisplayNameTextByIndex(dimensionPrefIdx);
		FString dimensionPrefDisplayString = dimensionPrefDisplayName.ToString();

		DimensionPrefDropdown->ComboBoxStringJustification->AddOption(dimensionPrefDisplayString);
		DimPrefsByDisplayString.Add(dimensionPrefDisplayString, dimensionPrefValue);
	}

	UpdateSettingsFromDoc();

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!IsDesignTime() && ensure(controller))
	{
		if (!controller->VoiceClient)
		{
			controller->OnVoiceClientConnected().AddUObject(this, &UModumateSettingsMenu::VoiceClientConnectedHandler);
		}
		else
		{
			VoiceClientConnectedHandler();
		}
	}

	VersionText->ChangeText(FText::FromString(UModumateFunctionLibrary::GetProjectVersion()));

	return true;
}

void UModumateSettingsMenu::UpdateSettingsFromDoc()
{
	auto* controller = GetOwningPlayer<AEditModelPlayerController>();
	auto* document = controller ? controller->GetDocument() : nullptr;
	if (document == nullptr)
	{
		return;
	}

	auto& currentSettings = document->GetCurrentSettings();

	if (!GetDimensionPrefFromTypes(currentSettings.DimensionType, currentSettings.DimensionUnit, CurDimensionPref))
	{
		return;
	}

	auto dimensionPrefEnum = StaticEnum<EDimensionPreference>();
	int32 curDimensionPrefIndex = dimensionPrefEnum->GetIndexByValue((int64)CurDimensionPref);
	DimensionPrefDropdown->ComboBoxStringJustification->SetSelectedIndex(curDimensionPrefIndex);

	CurDistIncrement = currentSettings.MinimumDistanceIncrement;
	PopulateDistIncrement();

	CurLatitude = currentSettings.Latitude;
	CurLongitude = currentSettings.Longitude;
	CurTrueNorth = currentSettings.TrueNorthDegree;
	TextEditableLatitude->ModumateEditableTextBox->SetText(FText::AsNumber(CurLatitude));
	TextEditableLongitude->ModumateEditableTextBox->SetText(FText::AsNumber(CurLongitude));
	TextEditableNorth->ModumateEditableTextBox->SetText(FText::AsNumber(CurTrueNorth));

	// Refresh current graphics settings values
	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance)
	{
		float maxValue = gameInstance->UserSettings.GraphicsSettings.UnrealGraphicsSettingsMaxValue;
		SliderGraphicShadows->SetValue(float(gameInstance->UserSettings.GraphicsSettings.Shadows) / maxValue);
		SliderGraphicAntiAliasing->SetValue(float(gameInstance->UserSettings.GraphicsSettings.AntiAliasing) / maxValue);
	}
}

bool UModumateSettingsMenu::GetDimensionTypesFromPref(EDimensionPreference DimensionPref, EDimensionUnits& OutDimensionType, EUnit& OutDimensionUnit)
{
	switch (DimensionPref)
	{
	case EDimensionPreference::FeetAndInches:
		OutDimensionType = EDimensionUnits::DU_Imperial;
		OutDimensionUnit = EUnit::Unspecified;
		return true;
	case EDimensionPreference::Meters:
		OutDimensionType = EDimensionUnits::DU_Metric;
		OutDimensionUnit = EUnit::Meters;
		return true;
	case EDimensionPreference::Millimeters:
		OutDimensionType = EDimensionUnits::DU_Metric;
		OutDimensionUnit = EUnit::Millimeters;
		return true;
	default:
		ensureMsgf(false, TEXT("Unhandled dimension preference: %d"), (int32)DimensionPref);
		return false;
	}
}

bool UModumateSettingsMenu::GetDimensionPrefFromTypes(EDimensionUnits DimensionType, EUnit DimensionUnit, EDimensionPreference& OutDimensionPref)
{
	switch (DimensionType)
	{
	case EDimensionUnits::DU_Imperial:
		OutDimensionPref = EDimensionPreference::FeetAndInches;
		break;
	case EDimensionUnits::DU_Metric:
		switch (DimensionUnit)
		{
		case EUnit::Meters:
			OutDimensionPref = EDimensionPreference::Meters;
			break;
		case EUnit::Millimeters:
			OutDimensionPref = EDimensionPreference::Millimeters;
			break;
		default:
			ensureMsgf(false, TEXT("Unhandled dimension unit: %d"), (int32)DimensionUnit);
			return false;
		}
		break;
	default:
		ensureMsgf(false, TEXT("Unknown dimension type: %d"), (int32)DimensionType);
		return false;
	}

	return true;
}

void UModumateSettingsMenu::OnDimensionPrefChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (DimPrefsByDisplayString.Contains(SelectedItem))
	{
		EDimensionPreference newDimensionPref = DimPrefsByDisplayString[SelectedItem];
		if (CurDimensionPref != newDimensionPref)
		{
			CurDimensionPref = newDimensionPref;
			CurDistIncrement = 0.0;
			PopulateDistIncrement();

			ApplyCurrentSettings();
		}
	}
}

void UModumateSettingsMenu::OnDistIncrementPrefChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (DistIncrementsByDisplayString.Contains(SelectedItem))
	{
		double newDistIncrement = DistIncrementsByDisplayString[SelectedItem];
		if (CurDistIncrement != newDistIncrement)
		{
			CurDistIncrement = newDistIncrement;
			ApplyCurrentSettings();
		}
	}
}

void UModumateSettingsMenu::OnMicDeviceChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!SelectedItem.IsEmpty() && SelectedItem != CurSelectedMic)
	{
		TMap<FString, FString> inputs;
		if (!GetAudioInputs(inputs))
		{
			return;
		}

		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		if (ensure(controller))
		{
			controller->VoiceClient->SetInputDevice(inputs[SelectedItem]);
			CurSelectedMic = SelectedItem;
		}
	}
}

void UModumateSettingsMenu::OnSpeakerDeviceChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!SelectedItem.IsEmpty() && SelectedItem != CurSelectedSpeaker)
	{
		TMap<FString, FString> outputs;
		if (!GetAudioOutputs(outputs))
		{
			return;
		}

		AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
		if (ensure(controller))
		{
			controller->VoiceClient->SetOutputDevice(outputs[SelectedItem]);
			CurSelectedSpeaker = SelectedItem;
		}
	}
}

void UModumateSettingsMenu::OnCloseButtonClicked()
{
	auto* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->ToggleSettingsWindow(false);
	}
}

void UModumateSettingsMenu::OnMouseCaptureEndSliderGraphicShadows()
{
	UpdateAndSaveGraphicsSettings();
}

void UModumateSettingsMenu::OnMouseCaptureEndSliderGraphicAntiAliasing()
{
	UpdateAndSaveGraphicsSettings();
}

void UModumateSettingsMenu::OnButtonAutoDetectSettingsReleased()
{
	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance)
	{
		return;
	}

	gameInstance->AutoDetectAndSaveModumateUserSettings();
	UpdateSettingsFromDoc();
}

void UModumateSettingsMenu::OnTextEditableLatitudeCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	CurLatitude = FCString::Atof(*Text.ToString());
	ApplyCurrentSettings();
}

void UModumateSettingsMenu::OnTextEditableLongitudeCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	CurLongitude = FCString::Atof(*Text.ToString());
	ApplyCurrentSettings();
}

void UModumateSettingsMenu::OnTextEditableNorthCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	CurTrueNorth = FCString::Atof(*Text.ToString());
	ApplyCurrentSettings();
}

void UModumateSettingsMenu::AudioDevicesChangedHandler()
{
	PopulateAudioDevices();
}

void UModumateSettingsMenu::PopulateAudioDevices()
{
	TMap<FString, FString> inputs;
	TMap<FString, FString> outputs;

	if (!(GetAudioOutputs(outputs) && GetAudioInputs(inputs)))
	{
		return;
	}

	//Clear and re-add the options...
	MicSourceDropdown->ComboBoxStringJustification->ClearOptions();
	for (auto input : inputs)
	{
		MicSourceDropdown->ComboBoxStringJustification->AddOption(input.Key);
	}

	SpeakerSourceDropdown->ComboBoxStringJustification->ClearOptions();
	for (auto output : outputs)
	{
		SpeakerSourceDropdown->ComboBoxStringJustification->AddOption(output.Key);
	}


	//Since we cleared, we have to set the selected option appropriately. If it does
	// not exist anymore, let's go ahead and set it to default.
	if (SpeakerSourceDropdown->ComboBoxStringJustification->FindOptionIndex(CurSelectedSpeaker) < 0)
	{
		SpeakerSourceDropdown->ComboBoxStringJustification->SetSelectedOption(DefaultSelection.ToString());
	}
	else
	{
		SpeakerSourceDropdown->ComboBoxStringJustification->SetSelectedOption(CurSelectedSpeaker);
	}

	if (MicSourceDropdown->ComboBoxStringJustification->FindOptionIndex(CurSelectedMic) < 0)
	{
		MicSourceDropdown->ComboBoxStringJustification->SetSelectedOption(DefaultSelection.ToString());
	}
	else
	{
		MicSourceDropdown->ComboBoxStringJustification->SetSelectedOption(CurSelectedMic);
	}
}

void UModumateSettingsMenu::VoiceClientConnectedHandler()
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!controller || !controller->VoiceClient)
	{
		return;
	}

	controller->VoiceClient->OnVoiceDevicesChanged().AddUObject(this, &UModumateSettingsMenu::AudioDevicesChangedHandler);
	PopulateAudioDevices();
}

void UModumateSettingsMenu::ApplyCurrentSettings()
{
	auto* controller = GetOwningPlayer<AEditModelPlayerController>();
	auto* document = controller ? controller->GetDocument() : nullptr;
	if (document == nullptr)
	{
		return;
	}

	auto& currentSettings = document->GetCurrentSettings();
	auto settingsDelta = MakeShared<FDocumentSettingDelta>(currentSettings, currentSettings);

	if (!GetDimensionTypesFromPref(CurDimensionPref, settingsDelta->NextSettings.DimensionType, settingsDelta->NextSettings.DimensionUnit))
	{
		return;
	}

	settingsDelta->NextSettings.MinimumDistanceIncrement = CurDistIncrement;
	settingsDelta->NextSettings.Latitude = CurLatitude;
	settingsDelta->NextSettings.Longitude = CurLongitude;
	settingsDelta->NextSettings.TrueNorthDegree = CurTrueNorth;

	// Don't bother applying deltas if it wouldn't result in any change
	if (settingsDelta->PreviousSettings == settingsDelta->NextSettings)
	{
		return;
	}

	document->ApplyDeltas({ settingsDelta }, GetWorld());
}

void UModumateSettingsMenu::PopulateDistIncrement()
{
	DistIncrementsByDisplayString.Reset();
	DistIncrementDropdown->ComboBoxStringJustification->ClearOptions();

	EDimensionUnits curDimensionType;
	EUnit curDimensionUnit;
	if (!GetDimensionTypesFromPref(CurDimensionPref, curDimensionType, curDimensionUnit))
	{
		return;
	}

	double minDistIncrement = 0.0;
	double defaultDistIncrement = 0.0;
	TArray<int32> distIncrementMultipliers;
	switch (curDimensionType)
	{
	case EDimensionUnits::DU_Imperial:
		minDistIncrement = FDocumentSettings::MinImperialDistIncrementCM;
		distIncrementMultipliers = FDocumentSettings::ImperialDistIncrementMultipliers;
		defaultDistIncrement = distIncrementMultipliers[FDocumentSettings::DefaultImperialMultiplierIdx] * minDistIncrement;
		break;
	case EDimensionUnits::DU_Metric:
		minDistIncrement = FDocumentSettings::MinMetricDistIncrementCM;
		distIncrementMultipliers = FDocumentSettings::MetricDistIncrementMultipliers;
		defaultDistIncrement = distIncrementMultipliers[FDocumentSettings::DefaultMetricMultiplierIdx] * minDistIncrement;
		break;
	}

	// If the dist increment is unset, then set its default value based on the current dimension type
	if (CurDistIncrement == 0.0)
	{
		CurDistIncrement = defaultDistIncrement;
	}

	for (int32 multiplierIdx = 0; multiplierIdx < distIncrementMultipliers.Num(); ++multiplierIdx)
	{
		int32 distIncrementMultiplier = distIncrementMultipliers[multiplierIdx];
		double distIncrement = minDistIncrement * distIncrementMultiplier;
		FText distIncrementText = UModumateDimensionStatics::CentimetersToDisplayText(distIncrement, 1, curDimensionType, curDimensionUnit);
		FString distIncrementString = distIncrementText.ToString();
		DistIncrementsByDisplayString.Add(distIncrementString, distIncrement);
		DistIncrementDropdown->ComboBoxStringJustification->AddOption(distIncrementString);

		if (distIncrement == CurDistIncrement)
		{
			DistIncrementDropdown->ComboBoxStringJustification->SetSelectedIndex(multiplierIdx);
		}
	}
}

void UModumateSettingsMenu::UpdateAndSaveGraphicsSettings()
{
	auto gameInstance = GetGameInstance<UModumateGameInstance>();
	if (!gameInstance)
	{
		return;
	}
	float maxValue = gameInstance->UserSettings.GraphicsSettings.UnrealGraphicsSettingsMaxValue;
	gameInstance->UserSettings.GraphicsSettings.Shadows = FMath::RoundToInt(SliderGraphicShadows->GetValue() * maxValue);
	gameInstance->UserSettings.GraphicsSettings.AntiAliasing = FMath::RoundToInt(SliderGraphicAntiAliasing->GetValue() * maxValue);
	gameInstance->UserSettings.SaveLocally();
	gameInstance->ApplyGraphicsFromModumateUserSettings();
}

bool UModumateSettingsMenu::GetAudioInputs(TMap<FString, FString> &OutInputs)
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!controller || !controller->VoiceClient)
	{
		return false;
	}

	return controller->VoiceClient->GetInputDevices(OutInputs);;
}

bool UModumateSettingsMenu::GetAudioOutputs(TMap<FString, FString> &OutOutputs)
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!controller || !controller->VoiceClient)
	{
		return false;
	}

	return controller->VoiceClient->GetOutputDevices(OutOutputs);
}