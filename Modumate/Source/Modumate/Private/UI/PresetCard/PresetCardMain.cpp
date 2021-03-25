// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardMain.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "UI/PresetCard/PresetCardHeader.h"
#include "UI/PresetCard/PresetCardPropertyList.h"
#include "UI/PresetCard/PresetCardObjectList.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Image.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/LeftMenu/BrowserItemObj.h"
#include "UI/PresetCard/PresetCardQuantityList.h"
#include "UI/PresetCard/PresetCardItemObject.h"
#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"

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
	
	if (CurrentPresetCardType == EPresetCardType::Browser)
	{
		// ItemObjs need to be updated so that widgets remain open during scrolling in listview
		ParentBrowserItemObj->bPresetCardExpanded = bExpandList;
		if (bExpandList)
		{
			BuildAsExpandedPresetCard(PresetGUID);
		}
		else
		{
			BuildAsCollapsedPresetCard(PresetGUID, true);
		}
		if (ensure(ParentNCPNavigator))
		{
			ParentNCPNavigator->RefreshDynamicMainListView();
		}
	}
	else if(CurrentPresetCardType == EPresetCardType::SelectTray)
	{
		ParentPresetCardItemObj->bPresetCardExpanded = bExpandList;
		if (bExpandList)
		{
			BuildAsExpandedPresetCard(PresetGUID);
		}
		else
		{
			BuildAsCollapsedPresetCard(PresetGUID, true);
		}
		if (ensure(ParentSelectionTrayBlockPresetList))
		{
			ParentSelectionTrayBlockPresetList->RefreshAssembliesListView();
		}
	}

}

void UPresetCardMain::BuildAsCollapsedPresetCard(const FGuid& InPresetKey, bool bInAllowInteraction)
{
	bAllowInteraction = bInAllowInteraction;
	PresetGUID = InPresetKey;

	ClearWidgetPool(DynamicVerticalBox);
	ClearWidgetPool(MainButton);

	UPresetCardHeader* newHeaderWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardHeader>(PresetCardHeaderCollapsedClass);
	if (newHeaderWidget)
	{
		MainButton->AddChild(newHeaderWidget);
		switch (CurrentPresetCardType)
		{
		case EPresetCardType::SelectTray:
			if (bBuildAsObjectTypeSelect)
			{
				newHeaderWidget->BuildAsSelectTrayPresetCardObjectType(SelectedObjectType, SelectCount);
			}
			else
			{
				newHeaderWidget->BuildAsSelectTrayPresetCard(PresetGUID, SelectCount);
			}
			break;
		case EPresetCardType::Swap:
			newHeaderWidget->BuildAsSwapHeader(PresetGUID, BIM_ID_NONE);
			break;
		case EPresetCardType::Browser:
		default:
			newHeaderWidget->BuildAsBrowserHeader(PresetGUID, BIM_ID_NONE); //TODO: Support non-assembly with NodeID?
			break;
		}
	}
	ToggleMainButtonInteraction(bAllowInteraction);

	// Set padding
	UBorderSlot* mainVerticalBoxSlot = UWidgetLayoutLibrary::SlotAsBorderSlot(MainVerticalBox);
	if (mainVerticalBoxSlot)
	{
		mainVerticalBoxSlot->SetPadding(MainVerticalBoxSlotCollapsedPadding);
	}

	// Drop shadow
	DropShadowImage->SetVisibility(ESlateVisibility::Collapsed);
}

void UPresetCardMain::BuildAsExpandedPresetCard(const FGuid& InPresetKey)
{
	PresetGUID = InPresetKey;

	ClearWidgetPool(DynamicVerticalBox);
	ClearWidgetPool(MainButton);

	UPresetCardHeader* newHeaderWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardHeader>(PresetCardHeaderSelectedClass);
	if (newHeaderWidget)
	{
		MainButton->AddChild(newHeaderWidget);
		switch (CurrentPresetCardType)
		{
		case EPresetCardType::SelectTray:
			if (bBuildAsObjectTypeSelect)
			{
				newHeaderWidget->BuildAsSelectTrayPresetCardObjectType(SelectedObjectType, SelectCount);
			}
			else
			{
				newHeaderWidget->BuildAsSelectTrayPresetCard(PresetGUID, SelectCount);
			}
			break;
		case EPresetCardType::Swap:
			newHeaderWidget->BuildAsSwapHeader(PresetGUID, BIM_ID_NONE);
			break;
		case EPresetCardType::Browser:
		default:
			newHeaderWidget->BuildAsBrowserHeader(PresetGUID, BIM_ID_NONE); //TODO: Support non-assembly with NodeID?
			break;
		}
	}
	ToggleMainButtonInteraction(true);

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

	UPresetCardQuantityList* newQuantityList = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardQuantityList>(PresetCardQuantityListClass);
	if (newQuantityList)
	{
		DynamicVerticalBox->AddChildToVerticalBox(newQuantityList);
		newQuantityList->BuildAsQuantityList(PresetGUID, false);
	}

	// Set padding
	UBorderSlot* mainVerticalBoxSlot = UWidgetLayoutLibrary::SlotAsBorderSlot(MainVerticalBox);
	if (mainVerticalBoxSlot)
	{
		mainVerticalBoxSlot->SetPadding(MainVerticalBoxSlotSelectedPadding);
	}

	// Drop shadow
	DropShadowImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPresetCardMain::SetAsNCPNavigatorPresetCard(class UNCPNavigator* InParentNCPNavigator, class UBrowserItemObj* InBrowserItemObj, EPresetCardType InPresetCardType)
{
	ParentNCPNavigator = InParentNCPNavigator;
	ParentBrowserItemObj = InBrowserItemObj;
	CurrentPresetCardType = InPresetCardType;
}

void UPresetCardMain::UpdateSelectionItemCount(int32 ItemCount)
{
	SelectCount = ItemCount;
	BuildAsCollapsedPresetCard(PresetGUID, true);
}

void UPresetCardMain::ToggleMainButtonInteraction(bool bEnable)
{
	if (bEnable)
	{
		MainButton->SetBackgroundColor(FLinearColor::White);
		SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		MainButton->SetBackgroundColor(bEnable ? FLinearColor::White : UserInteractionDisableBGColor);
		SetVisibility(bEnable ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
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
	ParentPresetCardItemObj = itemObj;
	PresetGUID = ParentPresetCardItemObj->PresetGuid;
	CurrentPresetCardType = ParentPresetCardItemObj->PresetCardType;
	if (CurrentPresetCardType == EPresetCardType::SelectTray)
	{
		// Find the preset for this list item. Note some item types do not require preset
		const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
		bBuildAsObjectTypeSelect = preset == nullptr;
		SelectedObjectType = ParentPresetCardItemObj->ObjectType;
		SelectCount = ParentPresetCardItemObj->SelectionItemCount;
		ParentSelectionTrayBlockPresetList = ParentPresetCardItemObj->ParentSelectionTrayBlockPresetList;
		if (ParentPresetCardItemObj->bPresetCardExpanded)
		{
			BuildAsExpandedPresetCard(PresetGUID);
		}
		else
		{
			BuildAsCollapsedPresetCard(PresetGUID, true);
		}
	}
}
