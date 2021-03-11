// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/NCPButton.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "UI/PresetCard/PresetCardMain.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/LeftMenu/NCPNavigator.h"

UNCPButton::UNCPButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNCPButton::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ModumateButton)
	{
		return false;
	}

	ModumateButton->OnReleased.AddDynamic(this, &UNCPButton::OnButtonPress);

	if (ButtonText)
	{
		ButtonText->SetText(ButtonTextOverride);
	}

	return true;
}

void UNCPButton::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UNCPButton::OnButtonPress()
{
	bExpanded = !bExpanded;
	ToggleTextColor(bExpanded);
	if (bExpanded)
	{
		BuildSubButtons();
	}
	else
	{
		EmptyLists();
	}
}

void UNCPButton::EmptyLists()
{
	for (auto& curWidget : DynamicNCPList->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	DynamicNCPList->ClearChildren();
}

void UNCPButton::BuildButton(class UNCPNavigator* InParentNCPNavigator, const FBIMTagPath& InNCP, int32 InTagOrder, bool bBuildAsExpanded)
{
	ParentNCPNavigator = InParentNCPNavigator;
	NCPTag = InNCP;
	TagOrder = InTagOrder;
	bExpanded = bBuildAsExpanded;
	ToggleTextColor(bExpanded);
	EmptyLists();

	// Add paddding to text
	UVerticalBoxSlot* verticalBoxSlot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(ButtonText);
	verticalBoxSlot->SetPadding(FMargin(TagOrder * TagPaddingSize, 0.f, 0.f, 0.f));

	if (ensureAlways(NCPTag.Tags.Num() > 0))
	{
		FBIMPresetTaxonomyNode taxNode;
		EMPlayerController->GetDocument()->GetPresetCollection().PresetTaxonomy.GetExactMatch(NCPTag, taxNode);
		if (ensureAlways(!taxNode.DisplayName.IsEmpty()))
		{
			ButtonText->SetText(taxNode.DisplayName);
		}
		else
		{
			FString lastTagString = InNCP.Tags[InNCP.Tags.Num() - 1];
			ButtonText->SetText(FText::FromString(lastTagString));
		}

		if (bExpanded)
		{
			BuildSubButtons();
		}
	}
}

void UNCPButton::BuildSubButtons()
{
	// Find subcategories under CurrentNCP at tag order
	FBIMTagPath partialPath;
	NCPTag.GetPartialPath(TagOrder + 1, partialPath);
	TArray<FString> subCats;
	EMPlayerController->GetDocument()->GetPresetCollection().GetNCPSubcategories(partialPath, subCats);

	// Build presets
	TArray<FGuid> availablePresets;
	EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(NCPTag, availablePresets, true);
	for (auto& newPreset : availablePresets)
	{
		if (ParentNCPNavigator->IsPresetAvailableForSearch(newPreset))
		{
			UPresetCardMain* newPresetWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardMain>(PresetCardMainClass);
			if (newPresetWidget)
			{
				DynamicNCPList->AddChildToVerticalBox(newPresetWidget);
				newPresetWidget->BuildAsBrowserCollapsedPresetCard(newPreset, true);
			}
		}
	}

	// Build a new NCP for each subcategory
	for (int32 i = 0; i < subCats.Num(); ++i)
	{
		FString partialPathString;
		partialPath.ToString(partialPathString);
		FString newPathString = partialPathString + FString::Printf(TEXT("_")) + subCats[i];
		FBIMTagPath newTagPath(newPathString);

		if (ParentNCPNavigator->IsNCPAvailableForSearch(newTagPath))
		{
			UNCPButton* newButton = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPButton>(NCPButtonClass);
			newButton->BuildButton(ParentNCPNavigator, newTagPath, TagOrder + 1);
			DynamicNCPList->AddChildToVerticalBox(newButton);
		}
	}
}

void UNCPButton::ToggleTextColor(bool bAsSelected)
{
	ButtonText->SetColorAndOpacity(bAsSelected ? SelectedColor : NonSelectedColor);
}
