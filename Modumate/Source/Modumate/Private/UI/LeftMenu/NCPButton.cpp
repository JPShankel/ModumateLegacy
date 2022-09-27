// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/NCPButton.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/PresetCard/PresetCardMain.h"
#include "Components/HorizontalBox.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/EditModelUserWidget.h"
#include "UI/TutorialMenu/HelpMenu.h"

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
	if (bAsTutorialArticle)
	{
		EMPlayerController->EditModelUserWidget->HelpMenuBP->ToArticleMenu(TutorialGUID);
		return;
	}

	if (ParentNCPNavigator)
	{
		ParentNCPNavigator->ToggleNCPTagAsSelected(NCPTag, !bExpanded);
	}
}

void UNCPButton::BuildCategoryButton(class UNCPNavigator* InParentNCPNavigator, const FBIMTagPath& InNCP, int32 InTagOrder, bool bBuildAsExpanded)
{
	ParentNCPNavigator = InParentNCPNavigator;
	bAsTutorialArticle = false;
	NCPTag = InNCP;
	TagOrder = InTagOrder;
	bExpanded = bBuildAsExpanded;
	ToggleTextColor(bExpanded);

	// Add paddding to text
	UButtonSlot* horizontalBoxSlot = Cast<UButtonSlot>(HorizontalBoxText->Slot);
	if (horizontalBoxSlot)
	{
		horizontalBoxSlot->SetPadding(FMargin(TagOrder * ButtonPaddingSizePerNCPorder, 0.f, 0.f, 0.f) + ButtonPaddingSizeOriginal);
	}
	ButtonIconGoToArticle->SetVisibility(ESlateVisibility::Collapsed);

	if (ensureAlways(NCPTag.Tags.Num() > 0))
	{
		FBIMPresetTaxonomyNode taxNode;
		EMPlayerController->GetDocument()->GetPresetCollection().PresetTaxonomy.GetExactMatch(NCPTag, taxNode);
		if (!taxNode.displayName.IsEmpty())
		{
			ButtonText->SetText(taxNode.displayName);
		}
		else
		{
			FString lastTagString = InNCP.Tags[InNCP.Tags.Num() - 1];
			ButtonText->SetText(FText::FromString(lastTagString));
		}
	}
}

void UNCPButton::BuildTutorialArticleButton(class UNCPNavigator* InParentNCPNavigator, const FGuid& InGUID, int32 InTagOrder)
{
	ParentNCPNavigator = InParentNCPNavigator;
	TutorialGUID = InGUID;
	bAsTutorialArticle = true;
	TagOrder = InTagOrder;
	ToggleTextColor(false);

	// Add paddding to text
	UButtonSlot* horizontalBoxSlot = Cast<UButtonSlot>(HorizontalBoxText->Slot);
	if (horizontalBoxSlot)
	{
		horizontalBoxSlot->SetPadding(FMargin(TagOrder * ButtonPaddingSizePerNCPorder, 0.f, 0.f, 0.f) + ButtonPaddingSizeOriginal);
	}
	ButtonIconGoToArticle->SetVisibility(ESlateVisibility::Visible);

	auto tutorialNode = EMPlayerController->EditModelUserWidget->HelpMenuBP->AllTutorialNodesByGUID.Find(TutorialGUID);
	if (tutorialNode)
	{
		ButtonText->SetText(FText::FromString(tutorialNode->Title));
	}
}

void UNCPButton::ToggleTextColor(bool bAsSelected)
{
	ButtonText->SetColorAndOpacity(bAsSelected ? SelectedColor : NonSelectedColor);
}
