// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMKey.h"
#include "BIMKernel/BIMNodeEditor.h"

/*

BIM CSV files are to be deprecated in favor of a SQL-based system
In the meantime, we read a manifest of CSV files into the preset database

The CSV files are organized by row with a command in the first cell and parameters in subsequent cells
The CSV reader processes rows one and at a time and produces node and preset definitions stored in FBIMPresetCollection
Row handling rules are defined in BIMKernel/BIMPresets in LoadCSVManifest

The TAGPATHS command defines not only tag paths, but a number of column ranges to be used by the PRESET command 
  to fill in properties, input pin assignments, slot configurations and other preset data.

When reading a preset, each cell is checked against the column ranges to see which one it falls into

Cells that fall into the PropertyRange define a property value, cells that fall into the PinRange define a child pin assignment, etc

Each column in a range has an associated string representing it's value in the TAGPATHS row

This is primarily used to store the name of a property that subsequent cells will assign a value to

*/

struct FBIMCSVReader
{
	struct FColumnRange
	{
		int32 first = -1;
		TArray<FString> columns;
		TArray<FBIMTagPath> columnTags;
		bool wantTags = false;
		bool IsIn(int32 i) const;
		const FString& Get(int32 i) const; 
	};

	FBIMCSVReader();

	FBIMPresetNodeType NodeType;

	TMap<FName, TFunction<UBIMPropertyBase*()>> PropertyFactoryMap;

	TMap<FBIMNameType, EBIMValueType> PropertyTypeMap;

	FBIMPreset Preset;

	FName PropertyClass;

	UBIMPropertyBase* UPropertySheet = nullptr;

	FColumnRange PropertyRange, MyPathRange, ParentPathRange, PinRange, IDRange, StartInProjectRange, SlotRange, PinChannelRange;

	ECraftingResult ProcessNodeTypeRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
	ECraftingResult ProcessPropertyDeclarationRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
	ECraftingResult ProcessTagPathRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
	ECraftingResult ProcessPresetRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TMap<FBIMKey, FBIMPreset>& OutPresets, TArray<FBIMKey>& OutStarters, TArray<FString>& OutMessages);
	ECraftingResult ProcessInputPinRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
};