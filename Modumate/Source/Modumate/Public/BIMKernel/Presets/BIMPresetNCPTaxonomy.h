// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMTagPath.h"
#include "Objects/ModumateObjectEnums.h"
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
	DisplayName,
	StopNCPTraversal
};

USTRUCT()
struct FBIMPresetTaxonomyNode
{
	GENERATED_BODY()

	UPROPERTY()
	FBIMTagPath tagPath;

	UPROPERTY()
	FText displayName;

	UPROPERTY()
	FText designerTitle;

	UPROPERTY()
	FText toolTip;

	UPROPERTY()
	EObjectType objectType = EObjectType::OTNone;

	UPROPERTY()
	EBIMAssetType assetType = EBIMAssetType::None;

	UPROPERTY()
	bool blockUpwardTraversal = false;

	UPROPERTY()
	EPresetMeasurementMethod measurementMethod = EPresetMeasurementMethod::None;
};

USTRUCT()
struct FBIMPresetNCPTaxonomy
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBIMPresetTaxonomyNode> nodes;

	EBIMResult LoadCSVRows(const FCsvParser::FRows& Rows);

	EBIMResult GetFirstPartialMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const;
	EBIMResult GetAllPartialMatches(const FBIMTagPath& TagPath, TArray<FBIMPresetTaxonomyNode>& OutNodes) const;
	EBIMResult GetExactMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const;
	int32 GetNodePosition(const FBIMTagPath& TagPath) const;
};