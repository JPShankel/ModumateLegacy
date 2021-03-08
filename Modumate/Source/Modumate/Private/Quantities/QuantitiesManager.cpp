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

	FString GetName() const
	{
		return Subname.IsEmpty() ? Name : Name + TEXT(" (") + Subname + TEXT(")");
	}
};

namespace
{
	int CategoryToPriority(const FText& Cat)
	{
		static const FText categories[] =
		{
			FText::FromString(TEXT("Wall")), FText::FromString(TEXT("Floor")),
			FText::FromString(TEXT("Ceiling")), FText::FromString(TEXT("Roof")),
			FText::FromString(TEXT("System Panel")), FText::FromString(TEXT("Stair")),
			FText::FromString(TEXT("Door")), FText::FromString(TEXT("Window")),
			FText::FromString(TEXT("Beam / Column")), FText::FromString(TEXT("Mullion")),
			FText::FromString(TEXT("Layer")), FText::FromString(TEXT("Module")),
			FText::FromString(TEXT("Gap")), FText::FromString(TEXT("Floor Cabinet")),
			FText::FromString(TEXT("FF&E")),
		};

		static constexpr int32 numCategories = sizeof (categories) / sizeof(FText);
		for (int32 i = 0; i < numCategories; ++i)
		{
			if (Cat.EqualTo(categories[i]))
			{
				return i;
			}
		}
		return numCategories;
	}
}

bool FQuantitiesManager::CreateReport(const FString& Filename)
{
	using FSubkey = FQuantityKey;
	TMap<FQuantityItemId, TArray<FQuantityKey>> itemToQuantityKeys;
	TMap<FQuantityItemId, TArray<FQuantityKey>> parentToQuantityKeys;
	TArray<FReportItem> topReportItems;

	auto gameInstance = GameInstance.Get();
	if (!gameInstance || !gameInstance->GetWorld())
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

	auto isTopLevelPortal = [](const FBIMPresetInstance* preset)
	{
		static const FText windowCategory = FText::FromString(TEXT("Window"));
		static const FText doorCategory = FText::FromString(TEXT("Door"));

		return preset->CategoryTitle.EqualTo(windowCategory)
			|| preset->CategoryTitle.EqualTo(doorCategory);
	};

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

		FReportItem reportItem;
		reportItem.Name = preset->DisplayName.ToString();
		reportItem.Subname = item.Key.Subname;
		reportItem.NameForSorting = reportItem.Name + item.Key;

		reportItem.ItemScope = preset->NodeScope;
		reportItem.Category = preset->CategoryTitle;
		reportItem.CategorySort = CategoryToPriority(reportItem.Category);

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
		topReportItems.Add(MoveTemp(reportItem));
	}

	topReportItems.Sort([](const FReportItem& a, const FReportItem& b)
	{
		return  a.CategorySort == b.CategorySort ?
			(a.Category.EqualTo(b.Category) ? a.NameForSorting < b.NameForSorting : a.Category.CompareTo(b.Category) < 0)
			: a.CategorySort < b.CategorySort;
	});

	PostProcessSizeGroups(topReportItems);

	// Now generate the CSV contents.
	FString csvContents;
	
	csvContents += TEXT("TOTAL ESTIMATES,,,,Count,ft^3,ft^2,ft,\n");

	FText currentCategory;
	for (auto& item: topReportItems)
	{
		csvContents += TEXT("\n");
		if (!item.Category.EqualTo(currentCategory))
		{
			currentCategory = item.Category;
			csvContents += currentCategory.ToString() + TEXT(",,,,,,,,\n");
		}
		csvContents += CsvEscape(item.GetName()) + TEXT(",,,,") + printCsvQuantities(item.Quantity) + TEXT(",\n");
		if (item.UsedIn.Num() > 0)
		{
			csvContents += FString(TEXT(",Used In Presets:,,,,,,,\n"));
			for (auto& usedInItem: item.UsedIn)
			{
				csvContents += FString(TEXT(",")) + CsvEscape(usedInItem.GetName()) + TEXT(",,,")
					+ printCsvQuantities(usedInItem.Quantity) + TEXT(",\n");
			}
		}
		if (item.Uses.Num() > 0)
		{
			csvContents += FString(TEXT(",Uses Presets:,,,,,,,\n"));
			for (auto& usesItem: item.Uses)
			{
				csvContents += FString(TEXT(",")) + CsvEscape(usesItem.GetName()) + TEXT(",,,")
					+ printCsvQuantities(usesItem.Quantity) + TEXT(",\n");
			}

		}
	}

	return FFileHelper::SaveStringToFile(csvContents, *Filename);
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
