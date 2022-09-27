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
#include "Components/VerticalBox.h"
#include "UI/TutorialMenu/HelpMenu.h"
#include "UI/TutorialMenu/HelpBlockTutorialSearch.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"

const FString UNCPNavigator::AnalyticsEventString = TEXT("NCPSearchString");

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
			canvasSlotBorder->SetAnchors(bBorderAutoSize ? EnableAutoSizeAnchor : DisableAutoSizeAnchor);
			canvasSlotBorder->SetOffsets(bBorderAutoSize ? EnableAutoSizeMargin : DisableAutoSizeMargin);
		}
	}

	if (!ButtonMarketplace)
	{
		return false;
	}
	ButtonMarketplace->ModumateButton->OnReleased.AddDynamic(this, &UNCPNavigator::OnReleaseButtonMarketplace);

	return true;
}

void UNCPNavigator::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
	EMGameState = GetWorld()->GetGameState<AEditModelGameState>();

	VerticalBox_TitleAndSearchbar->SetVisibility(bUsesHelpMenuSearchbar ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UNCPNavigator::BuildAssemblyList(bool bScrollToSelectedAssembly /*= false*/)
{
	if (!EMPlayerController || !EMPlayerController->CurrentTool)
	{
		return;
	}

	BuildNCPNavigator(EPresetCardType::AssembliesList);
	if (bScrollToSelectedAssembly)
	{
		FGuid toolGuid = EMPlayerController->CurrentTool->GetAssemblyGUID();
		FBIMTagPath ncpForSwap;
		EMPlayerController->GetDocument()->GetPresetCollection().GetNCPForPreset(toolGuid, ncpForSwap);
		if (ncpForSwap.Tags.Num() > 0)
		{
			ResetSelectedAndSearchTag();
			SetNCPTagPathAsSelected(ncpForSwap);
			ScrollPresetToView(toolGuid);
		}
	}
}

void UNCPNavigator::BuildNCPNavigator(EPresetCardType BuildAsType)
{
	CurrentPresetCardType = BuildAsType;
	DynamicMainListView->ClearListItems();

	const auto& starterTagStrings = CurrentPresetCardType == EPresetCardType::TutorialCategory || CurrentPresetCardType == EPresetCardType::TutorialArticle ? 
		TutorialNCPTagStrings : StarterNCPTagStrings;

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
		for (auto& curNCPTagString : starterTagStrings)
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
				bSourceTagIsOpen = starterTagStrings.FindRef(tagString);
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
	bool bBuildingAsTutorial = CurrentPresetCardType == EPresetCardType::TutorialCategory || CurrentPresetCardType == EPresetCardType::TutorialArticle;

	// Build presets
	TArray<FGuid> availableGUIDs;
	if (bBuildingAsTutorial)
	{
		EMPlayerController->EditModelUserWidget->HelpMenuBP->GetTutorialsForNCP(ParentNCP, availableGUIDs, true);
	}
	else
	{
		EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(ParentNCP, availableGUIDs, true);
	}

	for (auto& newGUID : availableGUIDs)
	{
		if (!IgnoredPresets.Contains(newGUID) && SearchFilteredPresets.Contains(newGUID))
		{
			UBrowserItemObj* newItemObj = NewObject<UBrowserItemObj>(this);
			newItemObj->ParentNCPNavigator = this;
			newItemObj->PresetGuid = newGUID;
			newItemObj->NCPTag = ParentNCP;
			newItemObj->TagOrder = TagOrder + 1;
			newItemObj->bPresetCardExpanded = false;

			newItemObj->bAsPresetCard = !bBuildingAsTutorial;
			newItemObj->PresetCardType = bBuildingAsTutorial ? EPresetCardType::TutorialArticle : CurrentPresetCardType;

			DynamicMainListView->AddItem(newItemObj);
		}
	}

	// Find subcategories under CurrentNCP at tag order
	FBIMTagPath partialPath;
	ParentNCP.GetPartialPath(TagOrder + 1, partialPath);
	TArray<FString> subCats;
	if (bBuildingAsTutorial)
	{
		EMPlayerController->EditModelUserWidget->HelpMenuBP->GetTutorialNCPSubcategories(partialPath, subCats);
	}
	else
	{
		EMPlayerController->GetDocument()->GetPresetCollection().GetNCPSubcategories(partialPath, subCats);
	}

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
			newItemObj->bAsPresetCard = false;
			newItemObj->NCPTag = newTagPath;
			newItemObj->TagOrder = TagOrder + 1;
			newItemObj->PresetCardType = bBuildingAsTutorial ? EPresetCardType::TutorialCategory : CurrentPresetCardType;
			DynamicMainListView->AddItem(newItemObj);

			// If NCP in subcategory is selected, that NCP is expanded
			if (SelectedTags.Contains(newTagPath) ||
				GetSearchTextBox()->ModumateEditableTextBox->GetText().ToString().Len() > 0)
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
	if (CurrentPresetCardType == EPresetCardType::TutorialArticle ||
		CurrentPresetCardType == EPresetCardType::TutorialCategory)
	{
		EMPlayerController->EditModelUserWidget->HelpMenuBP->GetTutorialsForNCP(NCPTag, availableBIMDesignerPresets, false);
	}
	else
	{
		EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(NCPTag, availableBIMDesignerPresets, false);
	}

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
	SearchFilteredPresets.Empty();
	FString searchSubString = GetSearchTextBox()->ModumateEditableTextBox->GetText().ToString();

	if (CurrentPresetCardType == EPresetCardType::TutorialArticle ||
		CurrentPresetCardType == EPresetCardType::TutorialCategory)
	{
		for (auto& curNCPTag : SourceNCPTags)
		{
			TArray<FGuid> availableTutorials;
			EMPlayerController->EditModelUserWidget->HelpMenuBP->GetTutorialsForNCP(curNCPTag, availableTutorials, false);
			for (auto& curTutorialGUID : availableTutorials)
			{
				// Is searchable if searchbar is empty
				if (searchSubString.Len() == 0)
				{
					SearchFilteredPresets.Add(curTutorialGUID);
				}
				else
				{
					auto tutorialNode = EMPlayerController->EditModelUserWidget->HelpMenuBP->AllTutorialNodesByGUID.Find(curTutorialGUID);
					if (tutorialNode)
					{
						// Is searchable if title or body contain searchbar text
						if (UKismetStringLibrary::Contains(tutorialNode->Title, searchSubString) ||
							UKismetStringLibrary::Contains(tutorialNode->Body, searchSubString))
						{
							SearchFilteredPresets.Add(curTutorialGUID);			
						}
						else
						{
							// Is searchable if any tags contain searchbar text 
							for (auto curTagString : tutorialNode->TagPath.Tags)
							{
								if (UKismetStringLibrary::Contains(curTagString, searchSubString))
								{
									SearchFilteredPresets.Add(curTutorialGUID);
								}
							}
						}
					}
				}
			}
		}
		return;
	}


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
	GetSearchTextBox()->ModumateEditableTextBox->SetText(FText::GetEmpty());
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
		bCanTraverse = TopTraversalNCP.Tags.Num() > 1 && !taxNode.blockUpwardTraversal;
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

void UNCPNavigator::SendAnalytics()
{
	if (!AnalyticsString.IsEmpty())
	{
		UModumateAnalyticsStatics::RecordEventCustomString(this, EModumateAnalyticsCategory::Presets, AnalyticsEventString, AnalyticsString);
		AnalyticsString.Empty();
	}
}

void UNCPNavigator::OnSearchBarChanged(const FText& NewText)
{
	if (!GetWorld() || !GetWorld()->GetFirstPlayerController())
	{
		return;
	}

	auto& timerManager = GetWorld()->GetFirstPlayerController()->GetWorldTimerManager();

	timerManager.ClearTimer(AnalyticsTimer);
	timerManager.SetTimer(AnalyticsTimer, this, &UNCPNavigator::SendAnalytics, 1.0f, false, 3.0f);
	AnalyticsString = NewText.ToString();

	BuildNCPNavigator(CurrentPresetCardType);
}

UModumateEditableTextBoxUserWidget* UNCPNavigator::GetSearchTextBox()
{
	if (bUsesHelpMenuSearchbar)
	{
		return EMPlayerController->EditModelUserWidget->HelpMenuBP->HelpBlockTutorialsSearchBP->ComponentSearchBar;
	}
	else
	{
		return SearchBarWidget;
	}
}

void UNCPNavigator::OnReleaseButtonMarketplace()
{
	FBIMTagPath curNCP;
	EMPlayerController->GetDocument()->GetPresetCollection().GetNCPForPreset(
	EMPlayerController->EditModelUserWidget->SwapMenuWidget->GetPresetGUIDToSwap(), curNCP);
	FBIMTagPath traverseNCP;
	GetTopTraversalPath(curNCP, traverseNCP);
	EMPlayerController->GetDocument()->OpenWebMarketplace(traverseNCP);
}
