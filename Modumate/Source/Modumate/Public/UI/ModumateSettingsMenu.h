// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "ModumateCore/ModumateDimensionString.h"

#include "ModumateSettingsMenu.generated.h"


// Helper enum for use only for displaying and selecting dimension type / unit override in the actual preferences.
UENUM()
enum class EDimensionPreference : uint8
{
	FeetAndInches,
	Meters,
	Millimeters
};

UCLASS(BlueprintType)
class MODUMATE_API UModumateSettingsMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	void UpdateFromCurrentSettings();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget* DimensionPrefDropdown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateDropDownUserWidget* DistIncrementDropdown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* CloseButton;

	static bool GetDimensionTypesFromPref(EDimensionPreference DimensionPref, EDimensionUnits& OutDimensionType, EUnit& OutDimensionUnit);
	static bool GetDimensionPrefFromTypes(EDimensionUnits DimensionType, EUnit DimensionUnit, EDimensionPreference& OutDimensionPref);

protected:
	UFUNCTION()
	void OnDimensionPrefChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnDistIncrementPrefChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnCloseButtonClicked();

	UPROPERTY()
	EDimensionPreference CurDimensionPref;

	UPROPERTY()
	double CurDistIncrement = 0.0;

	void ApplyCurrentSettings();
	void PopulateDistIncrement();

	TMap<FString, EDimensionPreference> DimPrefsByDisplayString;
	TMap<FString, double> DistIncrementsByDisplayString;
};
