// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Quantities/QuantitiesManager.h"
#include "Quantities/QuantitiesDimensions.h"
#include "UnrealClasses/EditModelGameState.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include <algorithm>

FQuantitiesManager::FQuantitiesManager(UModumateGameInstance* GameInstanceIn)
	: GameInstance(GameInstanceIn)
{ }

FQuantitiesManager::~FQuantitiesManager() = default;

bool FQuantitiesManager::CalculateAllQuantities()
{
	if (!bQuantitiesDirty)
	{
		return true;
	}

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
	FQuantitiesCollection quantities;
	CurrentQuantities.Reset(new FQuantitiesCollection);

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

	ProcessQuantityTree();

	SetDirtyBit(false);

	return !bool(errorMoi);
}

void FQuantitiesManager::ProcessQuantityTree()
{
	const FQuantitiesCollection::QuantitiesMap& quantitiesMap = CurrentQuantities->GetQuantities();
	AllQuantities.Empty();
	UsedByQuantities.Empty();
	UsesQuantities.Empty();
	ItemsByGuid.Empty();

	TMap<FQuantityItemId, TArray<FQuantityKey>> itemToQuantityKeys;
	TMap<FQuantityItemId, TArray<FQuantityKey>> parentToQuantityKeys;

	for (auto& quantity: quantitiesMap)
	{
		itemToQuantityKeys.FindOrAdd(quantity.Key.Item).Add(quantity.Key);
		parentToQuantityKeys.FindOrAdd(quantity.Key.Parent).Add(quantity.Key);
		ItemsByGuid.FindOrAdd(quantity.Key.Item.Id).Add(quantity.Key.Item);
	}

	for (const auto& item: itemToQuantityKeys)
	{
		FQuantity& allQuantity = AllQuantities.FindOrAdd(item.Key);
		auto& usedByQuantity = UsedByQuantities.FindOrAdd(item.Key);
		for (const FQuantityKey& itemKey: item.Value)
		{
			const FQuantity& oneQuantity = quantitiesMap[itemKey];
			allQuantity += oneQuantity;
			if (itemKey.Parent.Id.IsValid())
			{
				usedByQuantity.FindOrAdd(itemKey.Parent) += oneQuantity;
				UsesQuantities.FindOrAdd(itemKey.Parent).FindOrAdd(item.Key) += oneQuantity;
			}
		}
	}
}

void FQuantitiesManager::GetQuantityTree(const TMap<FQuantityItemId, FQuantity>*& OutAllQuantities,
	const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>*& OutUsedByQuantities,
	const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>*& OutUsesQuantities)
{
	CalculateAllQuantities();

	OutAllQuantities = &AllQuantities;
	OutUsedByQuantities = &UsedByQuantities;
	OutUsesQuantities = &UsesQuantities;
}

void FQuantitiesManager::GetWebQuantities(TArray<FString>& OutUsedBy, TArray<FString>& OutUses)
{
	CalculateAllQuantities();

	for (auto preset : UsedByQuantities)
	{
		OutUsedBy.Add(preset.Key.Id.ToString());
	}
	for (auto preset : UsesQuantities)
	{
		OutUses.Add(preset.Key.Id.ToString());
	}
}

TArray<FQuantityItemId> FQuantitiesManager::GetItemsForGuid(const FGuid& PresetId)
{
	CalculateAllQuantities();

	const auto* itemList = ItemsByGuid.Find(PresetId);
	if (itemList)
	{
		return itemList->Array();
	}
	else
	{
		return TArray<FQuantityItemId>();
	}
}

// Report helper class
struct FQuantitiesManager::FReportItem
{
	FString Name;
	FString Subname;
	FString NameForSorting;
	FGuid Id;
	EBIMValueScope ItemScope;
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

struct FQuantitiesManager::FNcpTree
{
	FString Tag;
	TArray<FNcpTree> Children;
	int32 Position = 0;  // Node position in taxonomy list for sorting.
	float MaterialCost = 0.0f;
	float LaborCost = 0.0f;
	TArray<int32> Items;

	bool operator==(const FString& Rhs) const { return Tag == Rhs; }

	void AddItem(int32 NewItemIndex, const FBIMTagPath& ItemPath, int32 SortIndex, int32 PathIndex = 0)
	{
		if (PathIndex == ItemPath.Tags.Num())
		{
			Items.Add(NewItemIndex);
		}
		else
		{
			auto* child = Children.FindByKey(ItemPath.Tags[PathIndex]);
			if (!child)
			{
				int32 newLocation = 0;
				for (const auto& c: Children)
				{
					if (SortIndex <= c.Position)
					{
						break;
					}
					++newLocation;
				}
				child = &Children.InsertDefaulted_GetRef(newLocation);
				child->Tag = ItemPath.Tags[PathIndex];
				child->Position = SortIndex;
			}
			child->AddItem(NewItemIndex, ItemPath, SortIndex, ++PathIndex);
		}
	}
};

bool FQuantitiesManager::CreateReport(const FString& Filename)
{
	using FSubkey = FQuantityKey;
	TMap<FQuantityItemId, TArray<FQuantityKey>> itemToQuantityKeys;
	TMap<FQuantityItemId, TArray<FQuantityKey>> parentToQuantityKeys;
	TArray<FReportItem> topReportItems;
	FNcpTree ncpTree;

	CalculateAllQuantities();

	auto gameInstance = GameInstance.Get();
	if (!gameInstance || !gameInstance->GetWorld() || !CurrentQuantities.IsValid())
	{
		return false;
	}

	auto world = gameInstance->GetWorld();
	UModumateDocument* doc = world->GetGameState<AEditModelGameState>()->Document;
	const FBIMPresetCollection& presets = doc->GetPresetCollection();
	auto& documentSettings = doc->GetCurrentSettings();
	bMetric = (documentSettings.DimensionType == EDimensionUnits::DU_Metric);
	const double linearScaleFactor = bMetric ? 100.0 : 30.48;
	const double areaScaleFactor = FMath::Pow(linearScaleFactor, 2);
	const double volumeScaleFactor = FMath::Pow(linearScaleFactor, 3);

	// Costs for each preset.
	ProcessCosts(presets);

	auto printCsvQuantities = [linearScaleFactor, areaScaleFactor, volumeScaleFactor](const FQuantity& Q)
	{
		return (Q.Count != 0.0f ? PrintNumber(Q.Count) : FString())
			+ TEXT(",") + (Q.Volume != 0.0f ? PrintNumber(Q.Volume / volumeScaleFactor) : FString())
			+ TEXT(",") + (Q.Area != 0.0f ? PrintNumber(Q.Area / areaScaleFactor) : FString())
			+ TEXT(",") + (Q.Linear != 0.0f ? PrintNumber(Q.Linear / linearScaleFactor) : FString());
	};

	const FQuantitiesCollection::QuantitiesMap& quantities = CurrentQuantities->GetQuantities();

	for (auto& item: AllQuantities)
	{
		const FGuid& itemGuid = item.Key.Id;
		const FBIMPresetInstance* preset = presets.PresetFromGUID(itemGuid);
		if (!ensure(preset))
		{
			continue;
		}

		FBIMTagPath itemNcp;
		presets.GetNCPForPreset(itemGuid, itemNcp);
		ensure(itemNcp.Tags.Num() > 0);

		FReportItem reportItem;
		reportItem.Name = preset->DisplayName.ToString();
		reportItem.Subname = item.Key.Subname;
		reportItem.NameForSorting = reportItem.Name + item.Key;
		reportItem.Id = item.Key.Id;

		reportItem.ItemScope = preset->NodeScope;
		reportItem.Quantity = item.Value;
		if (reportItem.ItemScope == EBIMValueScope::Module)
		{
			reportItem.Quantity.Area = 0.0f;
		}

		// 'Used By Presets:'
		TMap<FQuantityItemId, FQuantity>* usedByPresets = UsedByQuantities.Find(item.Key);
		if (usedByPresets)
		{
			for (auto& usedByPreset: *usedByPresets)
			{
				const FBIMPresetInstance* parentPreset = presets.PresetFromGUID(usedByPreset.Key.Id);
				if (!ensure(parentPreset))
				{
					continue;
				}
				FReportItem subItem;
				subItem.Name = parentPreset->DisplayName.ToString();
				subItem.Subname = usedByPreset.Key.Subname;
				subItem.Quantity = usedByPreset.Value;
				subItem.Id = usedByPreset.Key.Id;
				reportItem.UsedIn.Add(MoveTemp(subItem));
			}

		}

		// 'Uses Presets:'
		TMap<FQuantityItemId, FQuantity>* usesPresets = UsesQuantities.Find(item.Key);
		if (usesPresets)
		{
			for (auto& usesPreset: *usesPresets)
			{
				const FBIMPresetInstance* childPreset = presets.PresetFromGUID(usesPreset.Key.Id);
				if (!ensure(childPreset))
				{
					continue;
				}
				FReportItem subItem;
				subItem.Name = childPreset->DisplayName.ToString();
				subItem.Subname = usesPreset.Key.Subname;
				subItem.Quantity = usesPreset.Value;
				subItem.Id = usesPreset.Key.Id;
				reportItem.Uses.Add(MoveTemp(subItem));
			}
		}

		int32 sortIndex = presets.PresetTaxonomy.GetNodePosition(itemNcp);
		if (ensure(sortIndex >= 0))
		{
			ncpTree.AddItem(topReportItems.Add(MoveTemp(reportItem)), itemNcp, sortIndex);
		}
	}

	int32 categoryIndent;
	categoryIndent = TreeDepth(ncpTree);
	--categoryIndent;

	auto commas = [](int32 n) { TArray<TCHAR> s; s.Init(*TEXT(","), n); return FString(n, s.GetData()); };

	FBIMTagPath currentTagPath;
	FString CsvHeaderLines;
	TArray<FReportItem> treeReportItems;

	TFunction<void(FNcpTree*,int32)> processTreeItems = [&](FNcpTree* tree, int32 depth)
	{
		for (auto& child: tree->Children)
		{
			currentTagPath.Tags.Add(child.Tag);
			CsvHeaderLines += commas(depth);
			FBIMPresetTaxonomyNode ncpNode;
			presets.PresetTaxonomy.GetExactMatch(currentTagPath, ncpNode);
			FString nodeName = ncpNode.displayName.ToString();
			CsvHeaderLines += CsvEscape(nodeName);
			if (child.MaterialCost + child.LaborCost != 0.0f)
			{
				CsvHeaderLines += commas(categoryIndent + 9 - depth) + PrintNumber(child.MaterialCost)
					+ TEXT(",,") + PrintNumber(child.LaborCost)
					+ TEXT(",") + PrintNumber(child.MaterialCost + child.LaborCost);
			}
			CsvHeaderLines += TEXT("\n");
			processTreeItems(&child, depth + 1);
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

	TFunction<void(FNcpTree*, float&, float&)> sumCosts = [&](FNcpTree* tree, float& material, float& labor)
	{
		float childrenMaterial = 0.0f, childrenLabor = 0.0f;
		for (auto& child : tree->Children)
		{
			float m, l;
			sumCosts(&child, m, l);
			childrenMaterial += m;
			childrenLabor += l;
		}
		for (int32 i : tree->Items)
		{
			FReportItem item = topReportItems[i];
			childrenMaterial += item.Quantity.MaterialCost;
			childrenLabor += item.Quantity.LaborCost;
		}
		tree->MaterialCost = childrenMaterial;
		tree->LaborCost = childrenLabor;
		material = childrenMaterial;
		labor = childrenLabor;
	};

	// Process hierarchical costs.
	float materialTotal = 0.0f, laborTotal = 0.0f;
	sumCosts(&ncpTree, materialTotal, laborTotal);

	// Walk tree creating new FReportItem list in treeReportItems.
	processTreeItems(&ncpTree, 0);

	// Process sized items (eg. portals).
	PostProcessSizeGroups(treeReportItems);

	// Now generate the CSV contents.
	FString csvContents;
	csvContents += TEXT("TOTAL ESTIMATES") + commas(categoryIndent + 4) +
		(bMetric ? TEXT("Count,m3,m2,m") : TEXT("Count,ft^3,ft^2,ft")) + TEXT(",Material Cost Rate,Material Cost,Labor Cost Rate,Labor Cost,Total Cost\n");

	if (materialTotal + laborTotal != 0.0f)
	{
		csvContents += TEXT("Totals") + commas(categoryIndent + 9) + PrintNumber(materialTotal)
			+ TEXT(",,") + PrintNumber(laborTotal) + TEXT(",")
			+ PrintNumber(materialTotal + laborTotal) + TEXT("\n");
	}

	for (auto& item : treeReportItems)
	{
		csvContents += TEXT("\n");
		csvContents += item.Preface;
		int32 depth = item.Indentlevel;
		csvContents += commas(depth) + CsvEscape(item.GetName()) + commas(categoryIndent - depth + 4) + printCsvQuantities(item.Quantity);
		if (item.Quantity.LaborCost + item.Quantity.MaterialCost != 0.0f)
		{
			csvContents += TEXT(",") + PrintCsvCosts(item, presets);
		}
		csvContents += TEXT("\n");

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

FQuantity FQuantitiesManager::QuantityForOnePreset(const FGuid& PresetId)
{
	CalculateAllQuantities();

	FQuantity quantity;

	const auto* guidItems = ItemsByGuid.Find(PresetId);
	if (guidItems)
	{
		for (const auto& itemId : *guidItems)
		{
			quantity += AllQuantities[itemId];
		}
	}

	return quantity;
}

float FQuantitiesManager::GetModuleUnitsInArea(const FBIMPresetInstance* Preset, const FLayerPatternModule* Module, float Area)
{
	float length = Module->ModuleExtents.X;
	float height = Module->ModuleExtents.Z;
	float unitArea = length * height;
	return FMath::IsFinite(unitArea) && unitArea != 0.0f ? Area / unitArea : 0.0f;
}

int32 FQuantitiesManager::TreeDepth(const FNcpTree& Tree)
{
	int32 depth = 0;
	for (const auto& child : Tree.Children)
	{
		depth = FMath::Max(depth, TreeDepth(child));
	}
	return depth + 1;
}

// For items that have subnames (eg. size-class) create a new header item
// and just give subnames for those items.
void FQuantitiesManager::PostProcessSizeGroups(TArray<FReportItem>& ReportItems)
{
	// Sort blocks of same NCP for determinism and to bring sized parts together.
	for (int32 i = 0; i < ReportItems.Num(); ++i)
	{
		int32 j = i;
		while (++j < ReportItems.Num())
		{
			if (!ReportItems[j].Preface.IsEmpty())
			{
				break;
			}
		}

		FString preface = ReportItems[i].Preface;
		ReportItems[i].Preface.Empty();

		// Using STL sort as I don't see a way for UE4 sort to just sort a subrange.
		std::sort(&ReportItems[i], &ReportItems[0] + j, [](const FReportItem& a, const FReportItem& b)
			{return a.NameForSorting < b.NameForSorting; });

		ReportItems[i].Preface = preface;
	}

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
			header.Quantity = sectionTotal;
			header.Preface = ReportItems[i].Preface;
			ReportItems[i].Preface.Empty();
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

void FQuantitiesManager::ProcessCosts(const FBIMPresetCollection& Presets)
{
	// Calculate inherent costs for each quantity in AllQuantities.
	for (auto& quantity: AllQuantities)
	{
		const FBIMPresetInstance* preset = Presets.PresetFromGUID(quantity.Key.Id);
		if (ensure(preset))
		{
			FBIMConstructionCost costRate;
			if (preset->TryGetCustomData(costRate))
			{
				quantity.Value.CalculateCosts(costRate);
			}
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

FString FQuantitiesManager::PrintNumber(float N)
{
	static constexpr TCHAR format[] = TEXT("%.1f");
	return CsvEscape(FString::Printf(format, N));
}

FString FQuantitiesManager::PrintCsvCosts(const FReportItem& Item, const FBIMPresetCollection& Presets)
{
	const FBIMPresetInstance* preset = Presets.PresetFromGUID(Item.Id);
	FString materialRate, laborRate;
	float scale = 1.0f;
	const FQuantity& Quantity = Item.Quantity;
	if (!bMetric)
	{
		if (Quantity.Volume != 0.0f)
		{
			scale = 2.8317e-2f;
		}
		else if (Quantity.Area != 0.0f)
		{
			scale = 9.2903e-2f;
		}
		else if (Quantity.Linear != 0.0f)
		{
			scale = 0.3048f;
		}
	}

	if (preset)
	{
		FBIMConstructionCost costRate;
		if (preset->TryGetCustomData(costRate))
		{
			if (costRate.MaterialCostRate != 0.0f)
			{
				materialRate = CsvEscape(FString::Printf(TEXT("%.3f"), costRate.MaterialCostRate * scale));
			}
			if (costRate.LaborCostRate)
			{
				laborRate = CsvEscape(FString::Printf(TEXT("%.3f"), costRate.LaborCostRate * scale));
			}
		}
	}

	FString costsString = materialRate + TEXT(",") + PrintNumber(Quantity.MaterialCost)
		+ TEXT(",") + laborRate + TEXT(",") + PrintNumber(Quantity.LaborCost) + TEXT(",")
		+ PrintNumber(Quantity.MaterialCost + Quantity.LaborCost);
	return costsString;
}
