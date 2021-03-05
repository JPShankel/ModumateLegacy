// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/ModumateGameInstance.h"
#include "Quantities/QuantitiesDimensions.h"

class FQuantitiesVisitor;
struct FBIMPresetInstance;
struct FLayerPatternModule;

class MODUMATE_API FQuantitiesManager
{
public:
	FQuantitiesManager(UModumateGameInstance* GameInstanceIn);
	~FQuantitiesManager();
	bool CalculateAllQuantities();
	// Create a CSV-format spreadsheet with quantity summations (MOD-379).
	bool CreateReport(const FString& Filename);

	static float GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area);

private:
	TWeakObjectPtr<UModumateGameInstance> GameInstance;
	TUniquePtr<FQuantitiesVisitor> CurrentQuantities;

	static FString CsvEscape(const FString& String);
};
