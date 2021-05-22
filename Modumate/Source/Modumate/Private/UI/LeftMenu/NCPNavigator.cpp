// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/LeftMenu/NCPButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelUserWidget.h"
#include "UI/LeftMenu/DeleteMenuWidget.h"
#include "UI/LeftMenu/SwapMenuWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UnrealClasses/EditModelGameState.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "Kismet/KismetStringLibrary.h"
#include "UI/LeftMenu/BrowserItemObj.h"
#include "Components/ListView.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"


UNCPNavigator::UNCPNavigator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNCPNavigator::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!SearchBarWidget)
	{
		return false;
	}

	if (SearchBarWidget)
	{
		SearchBarWidget->ModumateEditableTextBox->OnTextChanged.AddDynamic(this, &UNCPNavigator::OnSearchBarChanged);
	}

	// AssembliesList require preset cards to autosize
	if (BorderBackground)
	{
		UCanvasPanelSlot* canvasSlotBorder = UWidgetLayoutLibrary::SlotAsCanvasSlot(BorderBackground);
		if (canvasSlotBorder)
		{
			canvasSlotBorder->bAutoSize = bBorderAutoSize;
		}
	}

	return true;
}

void UNCPNavigator::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
	EMGameState = GetWorld()->GetGameState<AEditModelGameState>();
}

void UNCPNavigator::BuildNCPNavigator(EPresetCardType BuildAsType)
{
	CurrentPresetCardType = BuildAsType;
	DynamicMainListView->ClearListItems();

	TArray<FBIMTagPath> sourceNCPTags;
	// Delete and swap navigator starts with NCPTraversal stopper
	// Other navigators start with browser like full list
	if (CurrentPresetCardType == EPresetCardType::Delete ||
		CurrentPresetCardType == EPresetCardType::Swap)
	{
		FBIMTagPath curNCP;
		if (CurrentPresetCardType == EPresetCardType::Delete)
		{
			// NCP from delete
			EMPlayerController->GetDocument()->GetPresetCollection().GetNCPForPreset(
				EMPlayerController->EditModelUserWidget->DeleteMenuWidget->GetPresetGUIDToDelete(), curNCP);
		}
		else
		{
			// NCP from swap
			EMPlayerController->GetDocument()->GetPresetCollection().GetNCPForPreset(
				EMPlayerController->EditModelUserWidget->SwapMenuWidget->GetPresetGUIDToSwap(), curNCP);
		}
		FBIMTagPath traverseNCP;
		GetTopTraversalPath(curNCP, traverseNCP);
		sourceNCPTags.Add(traverseNCP);
	}
	else
	{
		// Full list
		for (auto& curNCPTagString : StarterNCPTagStrings)
		{
			FBIMTagPath newTagPath;
			newTagPath.FromString(curNCPTagString.Key);
			sourceNCPTags.Add(newTagPath);
		}
	}

	CacheSearchFilteredPresets(sourceNCPTags);

	for (auto& curSourceNCPTag : sourceNCPTags)
	{
		if (IsNCPAvailableForSearch(curSourceNCPTag))
		{
			UBrowserItemObj* newAssemblyItemObj = NewObject<UBrowserItemObj>(this);
			newAssemblyItemObj->ParentNCPNavigator = this;
			newAssemblyItemObj->PresetCardType = CurrentPresetCardType;
			newAssemblyItemObj->bAsPresetCard = false;
			newAssemblyItemObj->NCPTag = curSourceNCPTag;
			newAssemblyItemObj->TagOrder = curSourceNCPTag.Tags.Num() - 1;

			// Check which NCP button should be opened
			bool bSourceTagIsOpen = SelectedTags.Contains(curSourceNCPTag);
			if (!bSourceTagIsOpen)
			{
				FString tagString;
				curSourceNCPTag.ToString(tagString);
				bSourceTagIsOpen = StarterNCPTagStrings.FindRef(tagString);
			}
			newAssemblyItemObj->bNCPButtonExpanded = bSourceTagIsOpen;
			DynamicMainListView->AddItem(newAssemblyItemObj);
			if (bSourceTagIsOpen)
			{
				BuildBrowserItemSubObjs(curSourceNCPTag, curSourceNCPTag.Tags.Num() - 1);
			}
		}
	}
}

void UNCPNavigator::BuildBrowserItemSubObjs(const FBIMTagPath& ParentNCP, int32 TagOrder)
{
	// Build presets
	TArray<FGuid> availablePresets;
	EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(ParentNCP, availablePresets, true);
	for (auto& newPreset : availablePresets)
	{
		if (!IgnoredPresets.Contains(newPreset) && SearchFilteredPresets.Contains(newPreset))
		{
			UBrowserItemObj* newItemObj = NewObject<UBrowserItemObj>(this);
			newItemObj->ParentNCPNavigator = this;
			newItemObj->PresetCardType = CurrentPresetCardType;
			newItemObj->bAsPresetCard = true;
			newItemObj->PresetGuid = newPreset;
			newItemObj->bPresetCardExpanded = false;
			DynamicMainListView->AddItem(newItemObj);
		}
	}

	// Find subcategories under CurrentNCP at tag order
	FBIMTagPath partialPath;
	ParentNCP.GetPartialPath(TagOrder + 1, partialPath);
	TArray<FString> subCats;
	EMPlayerController->GetDocument()->GetPresetCollection().GetNCPSubcategories(partialPath, subCats);

	// Build a new NCP for each subcategory
	for (int32 i = 0; i < subCats.Num(); ++i)
	{
		FString partialPathString;
		partialPath.ToString(partialPathString);
		FString newPathString = partialPathString + FString(TEXT("_")) + subCats[i];
		FBIMTagPath newTagPath(newPathString);

		if (IsNCPAvailableForSearch(newTagPath))
		{
			UBrowserItemObj* newItemObj = NewObject<UBrowserItemObj>(this);
			newItemObj->ParentNCPNavigator = this;
			newItemObj->PresetCardType = CurrentPresetCardType;
			newItemObj->bAsPresetCard = false;
			newItemObj->NCPTag = newTagPath;
			newItemObj->TagOrder = TagOrder + 1;
			DynamicMainListView->AddItem(newItemObj);

			// If NCP in subcategory is selected, that NCP is expanded
			if (SelectedTags.Contains(newTagPath) ||
				SearchBarWidget->ModumateEditableTextBox->GetText().ToString().Len() > 0)
			{
				newItemObj->bNCPButtonExpanded = true;
				BuildBrowserItemSubObjs(newTagPath, TagOrder + 1);
			}
			else
			{
				newItemObj->bNCPButtonExpanded = false;
			}
		}
	}
}

bool UNCPNavigator::IsNCPAvailableForSearch(const FBIMTagPath& NCPTag)
{
	TArray<FGuid> availableBIMDesignerPresets;
	EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(NCPTag, availableBIMDesignerPresets, false);

	// This NCP is available if its presets is available for search
	for (auto& curPreset : availableBIMDesignerPresets)
	{
		if (SearchFilteredPresets.Contains(curPreset))
		{
			return true;
		}
	}
	return false;
}

void UNCPNavigator::CacheSearchFilteredPresets(const TArray<FBIMTagPath>& SourceNCPTags)
{
	SearchFilteredPresets.Reset();
	FString searchSubString = SearchBarWidget->ModumateEditableTextBox->GetText().ToString();
	EObjectType currentObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(EMPlayerController->GetToolMode());

	// Preset is available if its display name contains string from searchbar
	for (auto& curNCPTag : SourceNCPTags)
	{
		TArray<FGuid> availableBIMDesignerPresets;
		EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(curNCPTag, availableBIMDesignerPresets, false);
		for (auto& curPreset : availableBIMDesignerPresets)
		{
			const FBIMPresetInstance* presetInst = EMGameState->Document->GetPresetCollection().PresetFromGUID(curPreset);
			if (presetInst)
			{
				// If this is an assembly list, use only the correct object type
				bool bFilteredFromAssemblyList = CurrentPresetCardType == EPresetCardType::AssembliesList && presetInst->ObjectType != currentObjectType;

				if (!bFilteredFromAssemblyList &&
					(searchSubString.Len() == 0 || (UKismetStringLibrary::Contains(presetInst->DisplayName.ToString(), searchSubString))))
				{
					SearchFilteredPresets.Add(presetInst->GUID);
				}
			}
		}
	}
}

void UNCPNavigator::ToggleNCPTagAsSelected(const FBIMTagPath& NCPTag, bool bAsSelected)
{
	if (bAsSelected)
	{
		SelectedTags.AddUnique(NCPTag);
	}
	else
	{
		SelectedTags.Remove(NCPTag);
	}
	BuildNCPNavigator(CurrentPresetCardType);
}

void UNCPNavigator::SetNCPTagPathAsSelected(const FBIMTagPath& NCPTag)
{
	for (int32 i = 0; i < NCPTag.Tags.Num(); ++i)
	{
		FBIMTagPath partialTag;
		NCPTag.GetPartialPath(i + 1, partialTag);
		ToggleNCPTagAsSelected(partialTag, true);
	}
}

void UNCPNavigator::RefreshDynamicMainListView()
{
	DynamicMainListView->RequestRefresh();
}

void UNCPNavigator::ResetSelectedAndSearchTag()
{
	SelectedTags.Reset();
	SearchBarWidget->ModumateEditableTextBox->SetText(FText::GetEmpty());
}

void UNCPNavigator::ScrollPresetToView(const FGuid PresetToView)
{
	for (auto& curItem : DynamicMainListView->GetListItems())
	{
		UBrowserItemObj* asBrowserItem = Cast<UBrowserItemObj>(curItem);
		if (asBrowserItem && asBrowserItem->PresetGuid == PresetToView)
		{
			DynamicMainListView->RequestScrollItemIntoView(curItem);
			return;
		}
	}
}

void UNCPNavigator::AddToIgnoredPresets(const TArray<FGuid>& InPresets)
{
	IgnoredPresets.Append(InPresets);
}

void UNCPNavigator::EmptyIgnoredPresets()
{
	IgnoredPresets.Empty();
}

void UNCPNavigator::GetTopTraversalPath(const FBIMTagPath& InNCP, FBIMTagPath& TopTraversalNCP)
{
	bool bCanTraverse = true;
	TopTraversalNCP = InNCP;
	while (bCanTraverse)
	{
		FBIMPresetTaxonomyNode taxNode;
		EMPlayerController->GetDocument()->GetPresetCollection().PresetTaxonomy.GetExactMatch(TopTraversalNCP, taxNode);
		bCanTraverse = TopTraversalNCP.Tags.Num() > 1 && taxNode.BlockUpwardTraversal;
		if (bCanTraverse)
		{
			FBIMTagPath partialPath;
			if (TopTraversalNCP.GetPartialPath(TopTraversalNCP.Tags.Num() - 1, partialPath) == EBIMResult::Success)
			{
				TopTraversalNCP = partialPath;
			}
			else
			{
				bCanTraverse = false;
			}
		}
	}
}

void UNCPNavigator::OnSearchBarChanged(const FText& NewText)
{
	BuildNCPNavigator(CurrentPresetCardType);
}
