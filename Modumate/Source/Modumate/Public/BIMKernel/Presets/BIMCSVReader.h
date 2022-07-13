// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMCSVReader.generated.h"

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


UENUM()
enum class ECSVMatrixNames : uint8
{
	Category,
	Color,
	ConstructionCost,
	Dimensions,
	FinalPartTransformAtSlot,
	GUID,
	ID,
	InputPins,
	Material,
	Mesh,
	MyCategoryPath,
	MiterPriority,
	Pattern,
	ParentCategoryPath,
	Profile,
	Properties,
	ReferencedAssets,
	Rig,
	Slots,
	StartsInProject,
	SubcategoryPath,
	EdgeDetail,
	PatternSegments,
	PatternSpans,
	PatternHostedElements,
	Error = 255
};

struct MODUMATE_API FBIMCSVReader
{
	struct FPresetMatrix
	{
		FPresetMatrix(ECSVMatrixNames InName, int32 InFirst) { Name = InName; First = InFirst; }

		ECSVMatrixNames Name;
		int32 First = -1;
		TArray<FString> Columns;

		bool IsIn(int32 i) const;
	};

	FBIMCSVReader();

	FBIMPresetTypeDefinition NodeType;
	FBIMPresetInstance Preset;

	TMap<FBIMNameType, EBIMValueType> PropertyTypeMap;

	static TMap<FBIMKey, FGuid> KeyGuidMap;
	static TMap<FGuid, FBIMKey> GuidKeyMap;

	TArray<FPresetMatrix> PresetMatrices;

	FString CurrentFile;

	template<class T>
	EBIMResult GetOrCreateCustomData(T& OutData)
	{
		if (!Preset.TryGetCustomData(OutData))
		{
			Preset.SetCustomData(OutData);
		}
		return EBIMResult::Success;
	}

	EBIMResult ProcessNodeTypeRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
	EBIMResult ProcessPropertyDeclarationRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
	EBIMResult ProcessTagPathRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
	EBIMResult ProcessPresetRow(const TArray<const TCHAR*>& Row, int32 RowNumber, FBIMPresetCollection& OutPresets, TArray<FGuid>& OutStarters, TArray<FString>& OutMessages);
	EBIMResult ProcessInputPinRow(const TArray<const TCHAR*>& Row, int32 RowNumber, TArray<FString>& OutMessages);
};