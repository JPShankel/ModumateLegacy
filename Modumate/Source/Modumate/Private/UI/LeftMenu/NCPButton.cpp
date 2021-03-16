// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/NCPButton.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "DocumentManagement/ModumateDocument.h"
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
	if (ParentNCPNavigator)
	{
		ParentNCPNavigator->ToggleNCPTagAsSelected(NCPTag, !bExpanded);
	}
}

void UNCPButton::BuildButton(class UNCPNavigator* InParentNCPNavigator, const FBIMTagPath& InNCP, int32 InTagOrder, bool bBuildAsExpanded)
{
	ParentNCPNavigator = InParentNCPNavigator;
	NCPTag = InNCP;
	TagOrder = InTagOrder;
	bExpanded = bBuildAsExpanded;
	ToggleTextColor(bExpanded);

	// Add paddding to text
	UVerticalBoxSlot* verticalBoxSlot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(ButtonText);
	verticalBoxSlot->SetPadding(FMargin(TagOrder * ButtonPaddingSizePerNCPorder, 0.f, 0.f, 0.f));

	if (ensureAlways(NCPTag.Tags.Num() > 0))
	{
		FBIMPresetTaxonomyNode taxNode;
		EMPlayerController->GetDocument()->GetPresetCollection().PresetTaxonomy.GetExactMatch(NCPTag, taxNode);
		if (!taxNode.DisplayName.IsEmpty())
		{
			ButtonText->SetText(taxNode.DisplayName);
		}
		else
		{
			FString lastTagString = InNCP.Tags[InNCP.Tags.Num() - 1];
			ButtonText->SetText(FText::FromString(lastTagString));
		}
	}
}

void UNCPButton::ToggleTextColor(bool bAsSelected)
{
	ButtonText->SetColorAndOpacity(bAsSelected ? SelectedColor : NonSelectedColor);
}
