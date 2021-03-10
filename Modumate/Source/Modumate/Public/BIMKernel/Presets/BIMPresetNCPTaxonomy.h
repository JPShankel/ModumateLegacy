// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMTagPath.h"
#include "Database/ModumateObjectEnums.h"
#include "Serialization/Csv/CsvParser.h"
#include "BIMPresetNCPTaxonomy.generated.h"

UENUM()
enum class EBIMTaxonomyColumn : uint8
{
	None = 0,
	ObjectType,
	AssetType,
	BIMDesignerNodeTitle,
	MeasurementMethods,
	DisplayName
};

USTRUCT()
struct FBIMPresetTaxonomyNode
{
	GENERATED_BODY()

	UPROPERTY()
	FBIMTagPath TagPath;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FText DesignerTitle;

	UPROPERTY()
	FText ToolTip;

	UPROPERTY()
	EObjectType ObjectType = EObjectType::OTNone;

	UPROPERTY()
	EBIMAssetType AssetType = EBIMAssetType::None;

	UPROPERTY()
	bool BlockUpwardTraversal = false;

	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;
};

USTRUCT()
struct FBIMPresetNCPTaxonomy
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBIMPresetTaxonomyNode> Nodes;

	EBIMResult LoadCSVRows(const FCsvParser::FRows& Rows);

	EBIMResult GetFirstPartialMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const;
	EBIMResult GetAllPartialMatches(const FBIMTagPath& TagPath, TArray<FBIMPresetTaxonomyNode>& OutNodes) const;
};