// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Quantities/QuantitiesManager.h"
#include "Quantities/QuantitiesVisitor.h"
#include "UnrealClasses/EditModelGameState.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"

FQuantitiesManager::FQuantitiesManager(UModumateGameInstance* GameInstanceIn)
	: GameInstance(GameInstanceIn)
{ }

FQuantitiesManager::~FQuantitiesManager() = default;

bool FQuantitiesManager::CalculateAllQuantities()
{
	auto gameInstance = GameInstance.Get();
	if (!gameInstance)
	{
		return false;
	}
	auto world = gameInstance->GetWorld();
	if (!world)
	{
		return false;
	}

	UModumateDocument* doc = world->GetGameState<AEditModelGameState>()->Document;
	FQuantitiesVisitor quantities(this);
	CurrentQuantities.Reset(new FQuantitiesVisitor(this));

	const auto& mois = doc->GetObjectInstances();
	const AModumateObjectInstance* errorMoi = nullptr;
	for (const auto* moi: mois)
	{
		if (!moi->ProcessQuantities(*CurrentQuantities))
		{
			errorMoi = moi;
			break;
		}
	}

	return !bool(errorMoi);
}

// Report helper class
struct FQuantitiesManager::FReportItem
{
	FString Name;
	FString Subname;
	FString NameForSorting;
	EBIMValueScope ItemScope;
	FText Category;
	int32 CategorySort;
	FQuantity Quantity;
	TArray<FReportItem> UsedIn;
	TArray<FReportItem> Uses;
	FString Preface;
	int32 Indentlevel = 0;

	FString GetName() const
	{
		return Subname.IsEmpty() ? Name : Name + TEXT(" (") + Subname + TEXT(")");
	}
};

namespace
{
	struct FNcpTree
	{
		TMap<FString, TUniquePtr<FNcpTree>> Children;
		TArray<int32> Items;
		FString NcpDisplayName;  // Display name for this leaf in the NCP tree.
		void AddItem(int32 NewItemIndex, const FBIMTagPath& ItemPath, int32 PathIndex = 0)
		{
			if (PathIndex == ItemPath.Tags.Num())
			{
				Items.Add(NewItemIndex);
			}
			else
			{
				auto child = Children.Find(ItemPath.Tags[PathIndex]);
				if (!child)
				{
					child = &Children.Add(ItemPath.Tags[PathIndex], MakeUnique<FNcpTree>());
				}
				(*child)->AddItem(NewItemIndex, ItemPath, ++PathIndex);
			}
		}
	};
}

bool FQuantitiesManager::CreateReport(const FString& Filename)
{
	using FSubkey = FQuantityKey;
	TMap<FQuantityItemId, TArray<FQuantityKey>> itemToQuantityKeys;
	TMap<FQuantityItemId, TArray<FQuantityKey>> parentToQuantityKeys;
	TArray<FReportItem> topReportItems;
	FNcpTree ncpTree;

	auto gameInstance = GameInstance.Get();
	if (!gameInstance || !gameInstance->GetWorld() || !CurrentQuantities.IsValid())
	{
		return false;
	}

	auto world = gameInstance->GetWorld();
	UModumateDocument* doc = world->GetGameState<AEditModelGameState>()->Document;
	const FBIMPresetCollection& presets = doc->GetPresetCollection();

	auto printCsvQuantities = [](const FQuantity& Q)
	{
		static constexpr TCHAR format[] = TEXT("%.1f");
		return (Q.Count != 0.0f ? FString::Printf(format, Q.Count) : FString())
			+ TEXT(",") + (Q.Volume != 0.0f ? FString::Printf(format, Q.Volume / 28316.8f) : FString())
			+ TEXT(",") + (Q.Area != 0.0f ? FString::Printf(format, Q.Area / 929.0f) : FString())
			+ TEXT(",") + (Q.Linear != 0.0f ? FString::Printf(format, Q.Linear / 30.48f) : FString());
	};

	const FQuantitiesVisitor::QuantitiesMap& quantities = CurrentQuantities->GetQuantities();

	for (auto& quantity: quantities)
	{
		itemToQuantityKeys.FindOrAdd(quantity.Key.Item).Add(quantity.Key);
		parentToQuantityKeys.FindOrAdd(quantity.Key.Parent).Add(quantity.Key);
	}

	for (auto& item: itemToQuantityKeys)
	{
		const FGuid& itemGuid = item.Key.Id;
		const FBIMPresetInstance* preset = presets.PresetFromGUID(itemGuid);
		if (!ensure(preset))
		{
			continue;
		}

		bool bReportUsesPresets = true;
		bool bReportUsedInPresets = true;

		FBIMTagPath itemNcp;
		presets.GetNCPForPreset(itemGuid, itemNcp);
		ensure(itemNcp.Tags.Num() > 0);

		FReportItem reportItem;
		reportItem.Name = preset->DisplayName.ToString();
		reportItem.Subname = item.Key.Subname;
		reportItem.NameForSorting = reportItem.Name + item.Key;

		reportItem.ItemScope = preset->NodeScope;
		reportItem.Category = preset->CategoryTitle;

		FQuantity accumulatedQuantity;
		// Loop over parent ('Used In') items:
		if (bReportUsedInPresets)
		{
			for (FQuantityKey& parent : item.Value)
			{
				FQuantity perParentQuantity = quantities[parent];
				accumulatedQuantity += perParentQuantity;
				if (reportItem.ItemScope == EBIMValueScope::Module)
				{
					perParentQuantity.Area = 0.0f;
				}

				if (parent.Parent.Id.IsValid())
				{
					const FBIMPresetInstance* parentPreset = presets.PresetFromGUID(parent.Parent.Id);
					if (!ensure(parentPreset))
					{
						continue;
					}
					FReportItem subItem;
					subItem.Name = parentPreset->DisplayName.ToString();
					subItem.Subname = parent.Parent.Subname;
					subItem.Quantity = perParentQuantity;
					reportItem.UsedIn.Add(MoveTemp(subItem));
				}
			}
		}

		if (reportItem.ItemScope == EBIMValueScope::Module)
		{
			accumulatedQuantity.Area = 0.0f;
		}
		reportItem.Quantity = accumulatedQuantity;

		const TArray<FQuantityKey>* usesPresets = parentToQuantityKeys.Find({ itemGuid, item.Key.Subname });
		// Loop over child ('Uses') items:
		if (usesPresets && bReportUsesPresets)
		{
			for (const FQuantityKey& usesPreset: *usesPresets)
			{
				const FBIMPresetInstance* childPreset = presets.PresetFromGUID(usesPreset.Item.Id);
				if (!ensure(childPreset))
				{
					continue;
				}
				FReportItem subItem;
				subItem.Name = childPreset->DisplayName.ToString();
				subItem.Subname = usesPreset.Item.Subname;
				subItem.Quantity = quantities[usesPreset];
				reportItem.Uses.Add(MoveTemp(subItem));
			}
		}
 		ncpTree.AddItem(topReportItems.Add(MoveTemp(reportItem)), itemNcp);
	}

	TFunction<void(FNcpTree*, int32&)> treeDepth = [&treeDepth](const FNcpTree* tree, int& depth)
	{
		int32 maxDepth = 0;
		for (const auto& child : tree->Children)
		{
			int32 childDepth;
			treeDepth(child.Value.Get(), childDepth);
			maxDepth = FMath::Max(maxDepth, childDepth);
		}
		depth = maxDepth + 1;
	};

	int32 categoryIndent;
	treeDepth(&ncpTree, categoryIndent);
	--categoryIndent;

	auto commas = [](int32 n) {FString s; return s.Append(TEXT(",,,,,,,,,,,,,,,,,,,,,"), n); };
	
	FBIMTagPath currentTagPath;
	FString CsvHeaderLines;
	TArray<FReportItem> treeReportItems;

	TFunction<void(FNcpTree*,int32)> processTreeItems = [&](FNcpTree* tree, int32 depth)
	{
		for (const auto& child: tree->Children)
		{
			currentTagPath.Tags.Add(child.Key);
			CsvHeaderLines += commas(depth);
			FString& nodeName = child.Value->NcpDisplayName;
			if (nodeName.IsEmpty())
			{
				FBIMPresetTaxonomyNode ncpNode;
				presets.PresetTaxonomy.GetExactMatch(currentTagPath, ncpNode);
				nodeName = ncpNode.DisplayName.ToString();
			}
			CsvHeaderLines += CsvEscape(nodeName) + TEXT("\n");
			processTreeItems(child.Value.Get(), depth + 1);
			currentTagPath.Tags.SetNum(currentTagPath.Tags.Num() - 1);
		}
		for (int32 i : tree->Items)
		{
			FReportItem item = topReportItems[i];
			if (!CsvHeaderLines.IsEmpty())
			{
				item.Preface = CsvHeaderLines;
				CsvHeaderLines.Empty();
			}
			item.Indentlevel = depth;
			treeReportItems.Add(item);
		}
	};

	// Walk tree creating new FReportItem list in treeReportItems.
	processTreeItems(&ncpTree, 0);

	// Process sized items (eg. portals).
	PostProcessSizeGroups(treeReportItems);

	// Now generate the CSV contents.
	FString csvContents;
	csvContents += TEXT("TOTAL ESTIMATES") + commas(categoryIndent + 4) + TEXT("Count,ft^3,ft^2,ft,\n");

	for (auto& item : treeReportItems)
	{
		csvContents += TEXT("\n");
		csvContents += item.Preface;
		int32 depth = item.Indentlevel;
		csvContents += commas(depth) + CsvEscape(item.GetName()) + commas(categoryIndent - depth + 4) + printCsvQuantities(item.Quantity) + TEXT(",\n");
		if (item.UsedIn.Num() > 0)
		{
			csvContents += commas(depth + 1) + FString(TEXT("Used In Presets:,,,,,,,\n"));
			for (auto& usedInItem : item.UsedIn)
			{
				csvContents += commas(depth + 1) + CsvEscape(usedInItem.GetName()) + commas(categoryIndent - depth + 3)
					+ printCsvQuantities(usedInItem.Quantity) + TEXT(",\n");
			}
		}
		if (item.Uses.Num() > 0)
		{
			csvContents += commas(depth + 1) + FString(TEXT("Uses Presets:,,,,,,,\n"));
			for (auto& usesItem : item.Uses)
			{
				csvContents += commas(depth + 1) + CsvEscape(usesItem.GetName()) + commas(categoryIndent - depth + 3)
					+ printCsvQuantities(usesItem.Quantity) + TEXT(",\n");
			}

		}
	}

	return FFileHelper::SaveStringToFile(csvContents, *Filename);
}

FQuantity FQuantitiesManager::QuantityForOnePreset(const FGuid& PresetId) const
{
	FQuantity quantity;
	if (CurrentQuantities.IsValid())
	{
		for (const auto& item : CurrentQuantities->GetQuantities())
		{
			if (item.Key.Item.Id == PresetId)
			{
				quantity += item.Value;
			}
		}
	}

	return quantity;
}

float FQuantitiesManager::GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area)
{
	float length = Module->ModuleExtents.X;
	float height = Module->ModuleExtents.Z;
	float unitArea =  length * height;
	return FMath::IsFinite(unitArea) && unitArea != 0.0f ? Area / unitArea : 0.0f;
}

// For items that have subnames (eg. size-class) create a new header item
// and just give subnames for those items.
void FQuantitiesManager::PostProcessSizeGroups(TArray<FReportItem>& ReportItems)
{
	for (int32 i = 0; i < ReportItems.Num(); ++i)
	{
		if (!ReportItems[i].Subname.IsEmpty())
		{
			int32 j = i;
			FQuantity sectionTotal;
			while (j < ReportItems.Num()
				&& ReportItems[i].Name == ReportItems[j].Name)
			{
				sectionTotal += ReportItems[j].Quantity;
				++j;
			}
			FReportItem& header = ReportItems.InsertDefaulted_GetRef(i++);
			header.Name = ReportItems[i].Name;
			header.NameForSorting = header.Name;
			header.Category = ReportItems[i].Category;
			header.CategorySort = ReportItems[i].CategorySort;
			header.Quantity = sectionTotal;
			header.Preface = ReportItems[i].Preface;
			header.Indentlevel = ReportItems[i].Indentlevel;
			for (; i <= j; ++i)
			{
				ReportItems[i].Name = ReportItems[i].Subname;
				ReportItems[i].Subname.Empty();
			}
			--i;
		}
	}
}

FString FQuantitiesManager::CsvEscape(const FString& String)
{
	if (String.Contains(TEXT("\"")) || String.Contains(TEXT(",")) || String.Contains(TEXT("\n"))
		|| String.StartsWith(TEXT(" ")) || String.EndsWith(TEXT(" ")) )
	{
		return FString(TEXT("\"")) + String.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}
	else
	{
		return String;
	}
}
