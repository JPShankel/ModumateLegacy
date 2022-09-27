// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetNCPTaxonomy.h"
#include "ModumateCore/EnumHelpers.h"
#include "Algo/Sort.h"

EBIMResult FBIMPresetNCPTaxonomy::LoadCSVRows(const FCsvParser::FRows& Rows)
{
	using TColumnRef = TPair<int32, EBIMTaxonomyColumn>;
	TArray<TColumnRef> columnTypes;

	for (int32 column = 1; column < Rows[0].Num(); ++column)
	{
		EBIMTaxonomyColumn columnType;
		if (FindEnumValueByName(Rows[0][column], columnType) && columnType != EBIMTaxonomyColumn::None)
		{
			columnTypes.Add(TColumnRef(column,columnType));
		}
	}

	for (int32 row = 1; row < Rows.Num(); ++row)
	{
		FString tagStr = FString(Rows[row][0]).Replace(TEXT(" "), TEXT(""));
		if (tagStr.IsEmpty())
		{
			continue;
		}

		FBIMTagPath tagPath;
		if (!ensureAlways(tagPath.FromString(tagStr) == EBIMResult::Success))
		{
			continue;
		}

		FBIMPresetTaxonomyNode& newNode = nodes.AddDefaulted_GetRef();
		newNode.tagPath = tagPath;

		for (auto& kvp : columnTypes)
		{
			FString cell = Rows[row][kvp.Key];
			if (cell.IsEmpty())
			{
				continue;
			}
			switch (kvp.Value)
			{
				case EBIMTaxonomyColumn::StopNCPTraversal:
					{
						// set to false by default, non-empty sets to true
						newNode.blockUpwardTraversal = true;
					}
					break;
				case EBIMTaxonomyColumn::ObjectType:
					if (!ensureAlways(FindEnumValueByName(*cell, newNode.objectType)))
					{
						return EBIMResult::Error;
					}
					break;

				case EBIMTaxonomyColumn::AssetType:
					if (!ensureAlways(FindEnumValueByName(*cell, newNode.assetType)))
					{
						return EBIMResult::Error;
					}
					break;

				case EBIMTaxonomyColumn::BIMDesignerNodeTitle:
					newNode.designerTitle = FText::FromString(cell);
					break;

				case EBIMTaxonomyColumn::MeasurementMethods:
					if (!ensureAlways(FindEnumValueByName(*cell, newNode.measurementMethod)))
					{
						return EBIMResult::Error;
					}
					break;

				case EBIMTaxonomyColumn::DisplayName:
					newNode.displayName = FText::FromString(cell);
					break;
			};

		}
	}

	// Order of entires in array used for quantity-estimates CSV file.

	return EBIMResult::Success;
}

EBIMResult FBIMPresetNCPTaxonomy::GetFirstPartialMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const
{
	for (auto& node : nodes)
	{
		if (node.tagPath.MatchesPartial(TagPath))
		{
			OutNode = node;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetNCPTaxonomy::GetAllPartialMatches(const FBIMTagPath& TagPath, TArray<FBIMPresetTaxonomyNode>& OutNodes) const
{
	for (auto& node : nodes)
	{
		if (node.tagPath.MatchesPartial(TagPath))
		{
			OutNodes.Add(node);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetNCPTaxonomy::GetExactMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const
{
	for (auto& node : nodes)
	{
		if (node.tagPath.MatchesExact(TagPath))
		{
			OutNode = node;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

int32 FBIMPresetNCPTaxonomy::GetNodePosition(const FBIMTagPath& TagPath) const
{
	for (const auto& node : nodes)
	{
		if (node.tagPath.MatchesExact(TagPath))
		{
			return &node - &nodes[0];
		}
	}
	return INDEX_NONE;
}
