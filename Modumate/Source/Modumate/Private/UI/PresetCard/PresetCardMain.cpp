// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardMain.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "UI/PresetCard/PresetCardHeader.h"
#include "UI/PresetCard/PresetCardPropertyList.h"
#include "UI/PresetCard/PresetCardObjectList.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/PresetCard/PresetCardItemObject.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Quantities/QuantitiesManager.h"

UPresetCardMain::UPresetCardMain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardMain::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!MainButton)
	{
		return false;
	}

	MainButton->OnReleased.AddDynamic(this, &UPresetCardMain::OnMainButtonReleased);

	return true;
}

void UPresetCardMain::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardMain::OnMainButtonReleased()
{
	bool bExpandList = DynamicVerticalBox->GetChildrenCount() == 0;
	if (bExpandList)
	{
		BuildAsBrowserSelectedPresetCard(PresetGUID);
	}
	else
	{
		BuildAsBrowserCollapsedPresetCard(PresetGUID, true);
	}
}

void UPresetCardMain::BuildAsBrowserCollapsedPresetCard(const FGuid& InPresetKey, bool bAllowInteraction)
{
	PresetGUID = InPresetKey;

	ClearWidgetPool(DynamicVerticalBox);
	ClearWidgetPool(MainButton);

	UPresetCardHeader* newHeaderWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardHeader>(PresetCardHeaderCollapsedClass);
	if (newHeaderWidget)
	{
		MainButton->AddChild(newHeaderWidget);
		newHeaderWidget->BuildAsBrowserHeader(PresetGUID, BIM_ID_NONE); //TODO: Support non-assembly with NodeID?
	}

	SetVisibility(bAllowInteraction ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);

	// Set padding
	UBorderSlot* mainBorderSlot = UWidgetLayoutLibrary::SlotAsBorderSlot(MainBorder);
	if (mainBorderSlot)
	{
		mainBorderSlot->SetPadding(MainBorderSlotCollapsedPadding);
	}
	UBorderSlot* mainVerticalBoxSlot = UWidgetLayoutLibrary::SlotAsBorderSlot(MainVerticalBox);
	if (mainVerticalBoxSlot)
	{
		mainVerticalBoxSlot->SetPadding(MainVerticalBoxSlotCollapsedPadding);
	}
}

void UPresetCardMain::BuildAsBrowserSelectedPresetCard(const FGuid& InPresetKey)
{
	PresetGUID = InPresetKey;

	ClearWidgetPool(DynamicVerticalBox);
	ClearWidgetPool(MainButton);

	UPresetCardHeader* newHeaderWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardHeader>(PresetCardHeaderSelectedClass);
	if (newHeaderWidget)
	{
		MainButton->AddChild(newHeaderWidget);
		newHeaderWidget->BuildAsBrowserHeader(PresetGUID, BIM_ID_NONE); //TODO: Support non-assembly with NodeID?
	}

	UPresetCardPropertyList* newPropertyListWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardPropertyList>(PresetCardPropertyListClass);
	if (newPropertyListWidget)
	{
		DynamicVerticalBox->AddChildToVerticalBox(newPropertyListWidget);
		newPropertyListWidget->BuildAsPropertyList(PresetGUID, false);
	}

	UPresetCardObjectList* newDescendentList = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardObjectList>(PresetCardObjectListClass);
	if (newDescendentList)
	{
		DynamicVerticalBox->AddChildToVerticalBox(newDescendentList);
		newDescendentList->BuildAsDescendentList(PresetGUID, false);
	}

	UPresetCardObjectList* newAncestorList = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardObjectList>(PresetCardObjectListClass);
	if (newAncestorList)
	{
		DynamicVerticalBox->AddChildToVerticalBox(newAncestorList);
		newAncestorList->BuildAsAncestorList(PresetGUID, false);
	}

	// Set padding
	UBorderSlot* mainBorderSlot = UWidgetLayoutLibrary::SlotAsBorderSlot(MainBorder);
	if (mainBorderSlot)
	{
		mainBorderSlot->SetPadding(MainBorderSlotSelectedPadding);
	}
	UBorderSlot* mainVerticalBoxSlot = UWidgetLayoutLibrary::SlotAsBorderSlot(MainVerticalBox);
	if (mainVerticalBoxSlot)
	{
		mainVerticalBoxSlot->SetPadding(MainVerticalBoxSlotSelectedPadding);
	}

	UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
	if (gameInstance && gameInstance->GetQuantitiesManager())
	{
		// TODO Quantites estimate
	}
}

void UPresetCardMain::ClearWidgetPool(class UPanelWidget* Widget)
{
	for (auto& curWidget : Widget->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	Widget->ClearChildren();
}

void UPresetCardMain::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	UPresetCardItemObject* itemObj = Cast<UPresetCardItemObject>(ListItemObject);
	if (!itemObj)
	{
		return;
	}

	BuildAsBrowserCollapsedPresetCard(itemObj->PresetGuid, true);
}
