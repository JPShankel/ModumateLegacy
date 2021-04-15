// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardMain.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Graph/Graph3DFace.h"
#include "UI/PresetCard/PresetCardHeader.h"
#include "UI/PresetCard/PresetCardPropertyList.h"
#include "UI/PresetCard/PresetCardObjectList.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Image.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/LeftMenu/BrowserItemObj.h"
#include "UI/PresetCard/PresetCardQuantityList.h"
#include "UI/PresetCard/PresetCardMetaDimension.h"
#include "UI/PresetCard/PresetCardItemObject.h"
#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/LeftMenu/SwapMenuWidget.h"
#include "UI/EditModelUserWidget.h"

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
	if (CurrentPresetCardType == EPresetCardType::Browser ||
		CurrentPresetCardType == EPresetCardType::Swap)
	{
		// ItemObjs need to be updated so that widgets remain open during scrolling in listview
		ParentBrowserItemObj->bPresetCardExpanded = !ParentBrowserItemObj->bPresetCardExpanded;
		if (ParentBrowserItemObj->bPresetCardExpanded)
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
		ParentPresetCardItemObj->bPresetCardExpanded = !ParentPresetCardItemObj->bPresetCardExpanded;
		if (ParentPresetCardItemObj->bPresetCardExpanded)
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
	else if (CurrentPresetCardType == EPresetCardType::AssembliesList)
	{
		EMPlayerController->EMPlayerState->SetAssemblyForToolMode(EMPlayerController->GetToolMode(), PresetGUID);
		if (ensure(ParentToolTrayBlockAssembliesList))
		{
			ParentToolTrayBlockAssembliesList->RefreshAssembliesListView();
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
				newHeaderWidget->BuildAsSelectTrayPresetCardObjectType(SelectedObjectType, SelectCount, bAllowInteraction);
			}
			else
			{
				newHeaderWidget->BuildAsSelectTrayPresetCard(PresetGUID, SelectCount, bAllowInteraction);
			}
			break;
		case EPresetCardType::Swap:
			if (EMPlayerController->EditModelUserWidget->SwapMenuWidget->CurrentSwapMenuType == ESwapMenuType::SwapFromSelection)
			{
				newHeaderWidget->BuildAsSwapHeader(PresetGUID, BIM_ID_NONE, bAllowInteraction);
			}
			else
			{
				newHeaderWidget->BuildAsSwapHeader(PresetGUID, EMPlayerController->EditModelUserWidget->SwapMenuWidget->BIMNodeIDToSwap, bAllowInteraction);
			}
			break;
		case EPresetCardType::Delete:
			newHeaderWidget->BuildAsDeleteHeader(PresetGUID, BIM_ID_NONE, bAllowInteraction); //TODO: Support non-assembly with NodeID?
			break;
		case EPresetCardType::AssembliesList:
			newHeaderWidget->BuildAsAssembliesListHeader(PresetGUID, bAllowInteraction);
			break;
		case EPresetCardType::Browser:
		default:
			newHeaderWidget->BuildAsBrowserHeader(PresetGUID, BIM_ID_NONE, bAllowInteraction); //TODO: Support non-assembly with NodeID?
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
			if (EMPlayerController->EditModelUserWidget->SwapMenuWidget->CurrentSwapMenuType == ESwapMenuType::SwapFromSelection)
			{
				newHeaderWidget->BuildAsSwapHeader(PresetGUID, BIM_ID_NONE);
			}
			else
			{
				newHeaderWidget->BuildAsSwapHeader(PresetGUID, EMPlayerController->EditModelUserWidget->SwapMenuWidget->BIMNodeIDToSwap);
			}
			break;
		case EPresetCardType::AssembliesList:
			newHeaderWidget->BuildAsAssembliesListHeader(PresetGUID);
			break;
		case EPresetCardType::Browser:
		default:
			newHeaderWidget->BuildAsBrowserHeader(PresetGUID, BIM_ID_NONE); //TODO: Support non-assembly with NodeID?
			break;
		}
	}
	ToggleMainButtonInteraction(true);

	// If the GUID is a preset, this is a MOI with an assembly so build property list
	if (EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID))
	{
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
	}
	// Otherwise, if this is a meta edge or plane, build a dimension card...meta vertices have no card
	else if (ParentPresetCardItemObj && (ParentPresetCardItemObj->ObjectType == EObjectType::OTMetaEdge || ParentPresetCardItemObj->ObjectType == EObjectType::OTMetaPlane))
	{
		UPresetCardMetaDimension* newDimensionWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardMetaDimension>(PresetCardMetaDimensionClass);
		if (newDimensionWidget)
		{
			DynamicVerticalBox->AddChildToVerticalBox(newDimensionWidget);
			TSet<AModumateObjectInstance*>& obs = EMPlayerController->EMPlayerState->SelectedObjects;
			auto* doc = EMPlayerController->GetDocument();
			if (ParentPresetCardItemObj->ObjectType == EObjectType::OTMetaPlane)
			{
				float totalArea = 0.0f;
				for (auto& ob : obs)
				{
					if (ob->GetAssembly().ObjectType == EObjectType::OTMetaPlane)
					{
						const Modumate::FGraph3DFace* face = doc->GetVolumeGraph().FindFace(ob->ID);
						if (ensureAlways(face != nullptr))
						{
							totalArea += face->CalculateArea();
						}
					}
				}
				newDimensionWidget->BuildAsMetaDimension(ParentPresetCardItemObj->ObjectType, totalArea);
			}
			else if (ParentPresetCardItemObj->ObjectType == EObjectType::OTMetaEdge)
			{
				float totalLength = 0.0f;
				for (auto& ob : obs)
				{
					if (ob->GetAssembly().ObjectType == EObjectType::OTMetaEdge && ob->GetNumCorners() > 1)
					{
						totalLength += (ob->GetCorner(0) - ob->GetCorner(1)).Size();
					}
				}
				newDimensionWidget->BuildAsMetaDimension(ParentPresetCardItemObj->ObjectType, totalLength);
			}
		}
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

bool UPresetCardMain::IsCurrentToolAssembly()
{
	FGuid currentAssembly = EMPlayerController->EMPlayerState->GetAssemblyForToolMode(EMPlayerController->GetToolMode());
	return currentAssembly == PresetGUID;
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
	else if (CurrentPresetCardType == EPresetCardType::AssembliesList)
	{
		ParentToolTrayBlockAssembliesList = ParentPresetCardItemObj->ParentToolTrayBlockAssembliesList;
		if (IsCurrentToolAssembly())
		{
			BuildAsExpandedPresetCard(PresetGUID);
		}
		else
		{
			BuildAsCollapsedPresetCard(PresetGUID, true);
		}
	}
}
