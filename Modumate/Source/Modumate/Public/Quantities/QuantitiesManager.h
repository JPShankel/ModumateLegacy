// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/ModumateGameInstance.h"
#include "Quantities/QuantitiesCollection.h"

class MODUMATE_API FQuantitiesManager
{
public:
	FQuantitiesManager(UModumateGameInstance* GameInstanceIn);
	~FQuantitiesManager();
	bool CalculateAllQuantities();
	void ProcessQuantityTree();
	void GetQuantityTree(const TMap<FQuantityItemId, FQuantity>*& OutAllQuantities,
		const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>*& OutUsedByQuantities,
		const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>*& OutUsesQuantities);
	void GetWebQuantities(TArray<FString>& OutUsedBy, TArray<FString>& OutUses);
	TArray<FQuantityItemId> GetItemsForGuid(const FGuid& PresetId);
	void SetDirtyBit(bool bValue = true) { bQuantitiesDirty = bValue; }

	// Create a CSV-format spreadsheet with quantity summations (MOD-379).
	bool CreateReport(const FString& Filename);
	FQuantity QuantityForOnePreset(const FGuid& PresetId);

	static float GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area);

	struct FReportItem;

private:
	struct FNcpTree;

	TWeakObjectPtr<UModumateGameInstance> GameInstance;
	TUniquePtr<FQuantitiesCollection> CurrentQuantities;

	bool bQuantitiesDirty = true;
	bool bMetric = false;

	// Processed state
	TMap<FQuantityItemId, FQuantity> AllQuantities;
	TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>> UsedByQuantities;
	TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>> UsesQuantities;
	TMap<FGuid, TSet<FQuantityItemId>> ItemsByGuid;

	int32 TreeDepth(const FNcpTree& Tree);
	void PostProcessSizeGroups(TArray<FReportItem>& ReportItems);
	void ProcessCosts(const FBIMPresetCollection& Presets);

	static FString CsvEscape(const FString& String);
	static FString PrintNumber(float N);
	FString PrintCsvCosts(const FReportItem& Item, const FBIMPresetCollection& Presets);
};
