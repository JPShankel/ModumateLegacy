// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardQuantityList.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Quantities/QuantitiesManager.h"
#include "UI/PresetCard/PresetCardQuantityListTotal.h"
#include "UI/PresetCard/PresetCardQuantityListSubTotal.h"
#include "UI/PresetCard/PresetCardQuantityListSubTotalListItem.h"

#define LOCTEXT_NAMESPACE "PresetCardQuantityList"

UPresetCardQuantityList::UPresetCardQuantityList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardQuantityList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!MainButton)
	{
		return false;
	}

	MainButton->OnReleased.AddDynamic(this, &UPresetCardQuantityList::OnMainButtonReleased);

	return true;
}

void UPresetCardQuantityList::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardQuantityList::OnMainButtonReleased()
{
	bool bExpandList = DynamicQuantityList->GetChildrenCount() == 0;
	BuildAsQuantityList(PresetGUID, bExpandList);
}

void UPresetCardQuantityList::BuildAsQuantityList(const FGuid& InGUID, bool bAsExpandedList)
{
	EmptyList();
	PresetGUID = InGUID;

	if (bAsExpandedList)
	{
		UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
		if (gameInstance && gameInstance->GetQuantitiesManager())
		{
			// Total quantity of this preset
			FQuantity totalQuantity = gameInstance->GetQuantitiesManager()->QuantityForOnePreset(PresetGUID);
			UPresetCardQuantityListTotal* newTotalWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardQuantityListTotal>(PresetCardQuantityListTotalClass);
			DynamicQuantityList->AddChildToVerticalBox(newTotalWidget);
			newTotalWidget->BuildTotalLabel(totalQuantity);

			// Get quantities for uses and used by
			const TMap<FQuantityItemId, FQuantity>* allQuantities;
			const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>* usedByQuantities;
			const TMap<FQuantityItemId, TMap<FQuantityItemId, FQuantity>>* usesQuantities;
			gameInstance->GetQuantitiesManager()->GetQuantityTree(allQuantities, usedByQuantities, usesQuantities);
			TArray<FQuantityItemId> itemIds = gameInstance->GetQuantitiesManager()->GetItemsForGuid(PresetGUID);

			// Uses
			UPresetCardQuantityListSubTotal* newUsesLabelWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardQuantityListSubTotal>(PresetCardQuantityListSubTotalClass);
			DynamicQuantityList->AddChildToVerticalBox(newUsesLabelWidget);
			newUsesLabelWidget->SetVisibility(ESlateVisibility::Collapsed);
			FText usesLabelText = LOCTEXT("UsesTitle", "Uses:");
			for (FQuantityItemId& curItemId : itemIds)
			{
				const TMap<FQuantityItemId, FQuantity>* usesMap = usesQuantities->Find(curItemId);
				if (usesMap != nullptr && usesMap->Num() > 0)
				{
					newUsesLabelWidget->SetVisibility(ESlateVisibility::Visible);
					for (auto& curQuantityItem : *usesMap)
					{
						UPresetCardQuantityListSubTotalListItem* newSubItem = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardQuantityListSubTotalListItem>(PresetCardQuantityListSubTotalListItemClass);
						DynamicQuantityList->AddChildToVerticalBox(newSubItem);
						newSubItem->BuildAsSubTotalListItem(curQuantityItem.Key, curQuantityItem.Value);
						newUsesLabelWidget->BuildSubLabel(usesLabelText, curQuantityItem.Value);
					}
				}
			}

			// Used By
			UPresetCardQuantityListSubTotal* newUsedByLabelWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardQuantityListSubTotal>(PresetCardQuantityListSubTotalClass);
			DynamicQuantityList->AddChildToVerticalBox(newUsedByLabelWidget);
			newUsedByLabelWidget->SetVisibility(ESlateVisibility::Collapsed);
			FText usedByLabelText = LOCTEXT("UsedByTitle", "Used By:");
			for (FQuantityItemId& curItemId : itemIds)
			{
				const TMap<FQuantityItemId, FQuantity>* usedByMap = usedByQuantities->Find(curItemId);
				if (usedByMap  != nullptr && usedByMap->Num() > 0)
				{
					newUsedByLabelWidget->SetVisibility(ESlateVisibility::Visible);
					for (auto& curQuantityItem : *usedByMap)
					{
						UPresetCardQuantityListSubTotalListItem* newSubItem = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardQuantityListSubTotalListItem>(PresetCardQuantityListSubTotalListItemClass);
						DynamicQuantityList->AddChildToVerticalBox(newSubItem);
						newSubItem->BuildAsSubTotalListItem(curQuantityItem.Key, curQuantityItem.Value);
						newUsedByLabelWidget->BuildSubLabel(usedByLabelText, curQuantityItem.Value);
					}
				}
			}
		}
	}
}

void UPresetCardQuantityList::EmptyList()
{
	for (auto& curWidget : DynamicQuantityList->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	DynamicQuantityList->ClearChildren();
}

#undef LOCTEXT_NAMESPACE