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

void UNCPButton::BuildButton(const FBIMTagPath& InNCP, int32 InTagOrder, bool bBuildAsExpanded)
{
	NCPTag = InNCP;
	TagOrder = InTagOrder;
	bExpanded = bBuildAsExpanded;
	ToggleTextColor(bExpanded);

	EmptyLists();

	if (ensureAlways(NCPTag.Tags.Num() > 0))
	{
		FString lastTagString = InNCP.Tags[InNCP.Tags.Num() - 1];
		ButtonText->SetText(FText::FromString(lastTagString));
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
	TArray<FGuid> AvailableBIMDesignerPresets;
	EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(NCPTag, AvailableBIMDesignerPresets, true);
	for (auto& newPreset : AvailableBIMDesignerPresets)
	{
		UPresetCardMain* newPresetWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardMain>(PresetCardMainClass);
		if (newPresetWidget)
		{
			DynamicNCPList->AddChildToVerticalBox(newPresetWidget);
			newPresetWidget->BuildAsBrowserCollapsedPresetCard(newPreset);
		}
	}

	// Build a new NCP for each subcategory
	for (int32 i = 0; i < subCats.Num(); ++i)
	{
		FString partialPathString;
		partialPath.ToString(partialPathString);
		FString newPathString = partialPathString + FString::Printf(TEXT("_")) + subCats[i];
		FBIMTagPath newTagPath(newPathString);

		UNCPButton* newButton = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPButton>(NCPButtonClass);
		newButton->BuildButton(newTagPath, TagOrder + 1);
		DynamicNCPList->AddChildToVerticalBox(newButton);
	}
}

void UNCPButton::ToggleTextColor(bool bAsSelected)
{
	ButtonText->SetColorAndOpacity(bAsSelected ? SelectedColor : NonSelectedColor);
}
