// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Quantities/QuantitiesManager.h"
#include "Quantities/QuantitiesVisitor.h"
#include "UnrealClasses/EditModelGameState.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include <algorithm>

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
			FString nodeName = ncpNode.DisplayName.ToString();
			CsvHeaderLines += CsvEscape(nodeName) + TEXT("\n");
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
