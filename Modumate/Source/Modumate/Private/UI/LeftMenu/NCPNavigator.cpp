// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/LeftMenu/NCPButton.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "UnrealClasses/EditModelGameState.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "Kismet/KismetStringLibrary.h"


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

	return true;
}

void UNCPNavigator::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
	EMGameState = GetWorld()->GetGameState<AEditModelGameState>();
}

void UNCPNavigator::BuildNCPNavigator()
{
	for (auto& curWidget : DynamicMainList->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	DynamicMainList->ClearChildren();

	// TODO: Blueprintable string array for root NCP?

	// Build first "Assembly" button in NCP
	FBIMTagPath rootPath;
	rootPath.FromString("Assembly");
	UNCPButton* newButton = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPButton>(NCPButtonClass);
	newButton->BuildButton(this, rootPath, 0, true);
	DynamicMainList->AddChildToVerticalBox(newButton);


	// Build first "Part" button in NCP
	FBIMTagPath partRootPath;
	partRootPath.FromString("Part");
	UNCPButton* newPartButton = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UNCPButton>(NCPButtonClass);
	newPartButton->BuildButton(this, partRootPath, 0, true);
	DynamicMainList->AddChildToVerticalBox(newPartButton);
}

bool UNCPNavigator::IsPresetAvailableForSearch(const FGuid& PresetGuid)
{
	FString searchSubString = SearchBarWidget->ModumateEditableTextBox->GetText().ToString();
	if (searchSubString.Len() == 0)
	{
		return true;
	}
	if (EMGameState)
	{
		const FBIMPresetInstance* preset = EMGameState->Document->GetPresetCollection().PresetFromGUID(PresetGuid);
		if (preset)
		{
			return UKismetStringLibrary::Contains(preset->DisplayName.ToString(), searchSubString);
		}
	}
	return false;
}

bool UNCPNavigator::IsNCPAvailableForSearch(const FBIMTagPath& NCPTag)
{
	TArray<FGuid> availableBIMDesignerPresets;
	EMPlayerController->GetDocument()->GetPresetCollection().GetPresetsForNCP(NCPTag, availableBIMDesignerPresets, false);
	for (auto& curPreset : availableBIMDesignerPresets)
	{
		if (IsPresetAvailableForSearch(curPreset))
		{
			return true;
		}
	}
	return false;
}

void UNCPNavigator::OnSearchBarChanged(const FText& NewText)
{
	BuildNCPNavigator();
}
