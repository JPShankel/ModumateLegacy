// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockNCPSwitcher.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Components/VerticalBox.h"
#include "UI/Custom/NCPSwitcherButton.h"
#include "UI/EditModelPlayerHUD.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/VerticalBoxSlot.h"

UBIMBlockNCPSwitcher::UBIMBlockNCPSwitcher(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockNCPSwitcher::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ButtonClose)
	{
		return false;
	}

	ButtonClose->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNCPSwitcher::OnButtonClosePress);

	return true;
}

void UBIMBlockNCPSwitcher::NativeConstruct()
{
	Super::NativeConstruct();
	Controller = GetOwningPlayer<AEditModelPlayerController>();
}

void UBIMBlockNCPSwitcher::OnButtonClosePress()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBIMBlockNCPSwitcher::BuildNCPSwitcher(const FBIMTagPath& InCurrentNCP)
{
	CurrentNCP = InCurrentNCP;

	SetVisibility(ESlateVisibility::Visible);

	// Release pool
	for (auto curWidget : VerticalBoxNCP->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			Controller->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}

	VerticalBoxNCP->ClearChildren();

	// Build first button in NCP
	FBIMTagPath rootPath;
	CurrentNCP.GetPartialPath(1, rootPath);
	UNCPSwitcherButton* newButton = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPSwitcherButton>(NCPSwitcherButtonClassSelected);
	newButton->BuildButton(rootPath);
	VerticalBoxNCP->AddChildToVerticalBox(newButton);

	// Build the rest using recursion. Keep building until the last tag
	AddNCPTagButtons(0);
}

void UBIMBlockNCPSwitcher::AddNCPTagButtons(int32 TagOrder)
{
	if (!ensureMsgf(TagOrder < 100, TEXT("TagOrder in AddNCPTagButtons at %d"), TagOrder))
	{
		return;
	}

	// Find subcategories under CurrentNCP at tag order
	FBIMTagPath partialPath;
	CurrentNCP.GetPartialPath(TagOrder + 1, partialPath);
	TArray<FString> subCats;
	Controller->GetDocument()->GetPresetCollection().GetNCPSubcategories(partialPath, subCats);

	// Build buttons for this tag's subcategories. Recursion ends here if there's none
	for (int32 i = 0; i < subCats.Num(); ++i)
	{
		// Build a new NCP for each button
		FString partialPathString;
		partialPath.ToString(partialPathString);
		FString newPathString = partialPathString + FString::Printf(TEXT("_")) + subCats[i];
		FBIMTagPath newTagPath(newPathString);

		// If this button is included in the CurrentNCP, this button should be build as selected
		bool buttonIsSelected = CurrentNCP.Tags.Contains(subCats[i]);
		TSubclassOf<UNCPSwitcherButton> ncpSwitcherButtonClass = buttonIsSelected ? NCPSwitcherButtonClassSelected : NCPSwitcherButtonClassDefault;
		UNCPSwitcherButton* newButton = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPSwitcherButton>(ncpSwitcherButtonClass);
		newButton->BuildButton(newTagPath);
		
		// Add the button to vertical box with padding
		VerticalBoxNCP->AddChildToVerticalBox(newButton);
		UVerticalBoxSlot* verticalBoxSlot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(newButton);
		verticalBoxSlot->SetPadding(FMargin((TagOrder + 1) * TagPaddingSize, 0.f, 0.f, 0.f));

		// Build more subcategory children if this button is selected
		if (buttonIsSelected)
		{
			AddNCPTagButtons(TagOrder + 1);
		}
	}
}
