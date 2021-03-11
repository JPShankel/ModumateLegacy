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

		FBIMPresetTaxonomyNode& newNode = Nodes.AddDefaulted_GetRef();
		newNode.TagPath = tagPath;

		for (auto& kvp : columnTypes)
		{
			FString cell = Rows[row][kvp.Key];
			if (cell.IsEmpty())
			{
				continue;
			}
			switch (kvp.Value)
			{
				case EBIMTaxonomyColumn::ObjectType:
					if (!ensureAlways(FindEnumValueByName(*cell, newNode.ObjectType)))
					{
						return EBIMResult::Error;
					}
					break;

				case EBIMTaxonomyColumn::AssetType:
					if (!ensureAlways(FindEnumValueByName(*cell, newNode.AssetType)))
					{
						return EBIMResult::Error;
					}
					break;

				case EBIMTaxonomyColumn::BIMDesignerNodeTitle:
					newNode.DesignerTitle = FText::FromString(cell);
					break;

				case EBIMTaxonomyColumn::MeasurementMethods:
					if (!ensureAlways(FindEnumValueByName(*cell, newNode.MeasurementMethod)))
					{
						return EBIMResult::Error;
					}
					break;

				case EBIMTaxonomyColumn::DisplayName:
					newNode.DisplayName = FText::FromString(cell);
					break;
			};

		}
	}

	// Sort so more specific matches are earlier in the list, ie Part->DoorKnob->Indoor will appear before Part->DoorKnob
	Algo::Sort(Nodes, [](const FBIMPresetTaxonomyNode& LHS, const FBIMPresetTaxonomyNode& RHS)
	{
		return LHS.TagPath.Tags.Num() > RHS.TagPath.Tags.Num();
	});

	return EBIMResult::Success;
}

EBIMResult FBIMPresetNCPTaxonomy::GetFirstPartialMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const
{
	for (auto& node : Nodes)
	{
		if (node.TagPath.MatchesPartial(TagPath))
		{
			OutNode = node;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetNCPTaxonomy::GetAllPartialMatches(const FBIMTagPath& TagPath, TArray<FBIMPresetTaxonomyNode>& OutNodes) const
{
	for (auto& node : Nodes)
	{
		if (node.TagPath.MatchesPartial(TagPath))
		{
			OutNodes.Add(node);
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetNCPTaxonomy::GetExactMatch(const FBIMTagPath& TagPath, FBIMPresetTaxonomyNode& OutNode) const
{
	for (auto& node : Nodes)
	{
		if (node.TagPath.MatchesExact(TagPath))
		{
			OutNode = node;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}
