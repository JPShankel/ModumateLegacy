// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/ModumateGameInstance.h"
#include "Quantities/QuantitiesDimensions.h"

class FQuantitiesVisitor;
struct FBIMPresetInstance;
struct FLayerPatternModule;
struct FQuantity;

class MODUMATE_API FQuantitiesManager
{
public:
	FQuantitiesManager(UModumateGameInstance* GameInstanceIn);
	~FQuantitiesManager();
	bool CalculateAllQuantities();
	// Create a CSV-format spreadsheet with quantity summations (MOD-379).
	bool CreateReport(const FString& Filename);
	FQuantity QuantityForOnePreset(const FGuid& PresetId) const;

	static float GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area);

private:
	TWeakObjectPtr<UModumateGameInstance> GameInstance;
	TUniquePtr<FQuantitiesVisitor> CurrentQuantities;

	struct FReportItem;
	void PostProcessSizeGroups(TArray<FReportItem>& ReportItems);

	static FString CsvEscape(const FString& String);
};
