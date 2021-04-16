// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/ModumateSettingsMenu.h"

#include "Components/ComboBoxString.h"
#include "DocumentManagement/DocumentSettings.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/EnumHelpers.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"


bool UModumateSettingsMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(DimensionPrefDropdown && DistIncrementDropdown && CloseButton))
	{
		return false;
	}

	DimensionPrefDropdown->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UModumateSettingsMenu::OnDimensionPrefChanged);
	DistIncrementDropdown->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UModumateSettingsMenu::OnDistIncrementPrefChanged);
	CloseButton->ModumateButton->OnClicked.AddDynamic(this, &UModumateSettingsMenu::OnCloseButtonClicked);

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

	UpdateFromCurrentSettings();

	return true;
}

void UModumateSettingsMenu::UpdateFromCurrentSettings()
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

void UModumateSettingsMenu::OnCloseButtonClicked()
{
	auto* controller = GetOwningPlayer<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->ToggleSettingsWindow(false);
	}
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

	// Don't bother applying deltas if it wouldn't result in any change
	if (settingsDelta->PreviousSettings == settingsDelta->NextSettings)
	{
		return;
	}

	document->ApplyDeltas({ settingsDelta }, GetWorld());
	UpdateFromCurrentSettings();
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
