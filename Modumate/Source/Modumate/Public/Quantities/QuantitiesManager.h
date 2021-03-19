// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/ModumateGameInstance.h"
#include "Quantities/QuantitiesVisitor.h"

class MODUMATE_API FQuantitiesManager
{
public:
	FQuantitiesManager(UModumateGameInstance* GameInstanceIn);
	~FQuantitiesManager();
	bool CalculateAllQuantities();
	void ProcessQuantityTree();
	void GetQuantityTree(const TMap<FQuantityItemId, FQuantity>*& OutAllQuantities,
		const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>*& OutUsedByQuantities,
		const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>*& OutUsesQuantities) const;
	TArray<FQuantityItemId> GetItemsForGuid(const FGuid& PresetId) const;

	// Create a CSV-format spreadsheet with quantity summations (MOD-379).
	bool CreateReport(const FString& Filename);
	FQuantity QuantityForOnePreset(const FGuid& PresetId) const;

	static float GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area);

	struct FReportItem;

private:
	struct FNcpTree;

	TWeakObjectPtr<UModumateGameInstance> GameInstance;
	TUniquePtr<FQuantitiesVisitor> CurrentQuantities;

	// Processed state
	TMap<FQuantityItemId, FQuantity> AllQuantities;
	TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>> UsedByQuantities;
	TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>> UsesQuantities;
	TMap<FGuid, TSet<FQuantityItemId>> ItemsByGuid;

	int32 TreeDepth(const FNcpTree& Tree);
	void PostProcessSizeGroups(TArray<FReportItem>& ReportItems);

	static FString CsvEscape(const FString& String);
};
