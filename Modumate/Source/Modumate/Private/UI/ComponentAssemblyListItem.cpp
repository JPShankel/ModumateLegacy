// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentAssemblyListItem.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/VerticalBox.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/ComponentPresetListItem.h"
#include "UI/ComponentListObject.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"
#include "UI/BIM/BIMDesigner.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Components/Image.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"

using namespace Modumate;

UComponentAssemblyListItem::UComponentAssemblyListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UComponentAssemblyListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ModumateButtonMain && ButtonEdit && ButtonSwap && ButtonConfirm))
	{
		return false;
	}

	ModumateButtonMain->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnModumateButtonMainReleased);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonEditReleased);
	ButtonSwap->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonSwapReleased);
	ButtonConfirm->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonConfirmReleased);

	return true;
}

void UComponentAssemblyListItem::NativeConstruct()
{
	Super::NativeConstruct();

	NormalButtonStyle = ModumateButtonMain->WidgetStyle;
	ActiveButtonStyle = ModumateButtonMain->WidgetStyle;
	ActiveButtonStyle.Normal = NormalButtonStyle.Pressed;
	ActiveButtonStyle.Hovered = NormalButtonStyle.Pressed;
}

void UComponentAssemblyListItem::UpdateItemType(EComponentListItemType NewItemType)
{
	ItemType = NewItemType;
	if (!(ButtonEdit && ButtonSwap && ButtonTrash && ButtonConfirm))
	{
		return;
	}

	ComponentPresetItem->IconImage->SetVisibility(bIsNonAssemblyObjectSelectItem ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ComponentPresetItem->IconImageBackground->SetVisibility(bIsNonAssemblyObjectSelectItem ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	// TODO: TextNumber and GrabHandleImage are collapsed for now until assembly list can be rearranged
	ComponentPresetItem->TextNumber->SetVisibility(ESlateVisibility::Collapsed);
	ComponentPresetItem->GrabHandleImage->SetVisibility(ESlateVisibility::Collapsed);

	switch (ItemType)
	{
	case EComponentListItemType::AssemblyListItem:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Visible);
		ButtonEdit->SetVisibility(ESlateVisibility::Visible);
		ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EComponentListItemType::SelectionListItem:
		ButtonSwap->SetVisibility(bIsNonAssemblyObjectSelectItem ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(bIsNonAssemblyObjectSelectItem ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EComponentListItemType::SwapListItem:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Collapsed);
		ButtonConfirm->SetVisibility(ESlateVisibility::Visible);
		break;
	case EComponentListItemType::SwapDesignerPreset:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Collapsed);
		ButtonConfirm->SetVisibility(ESlateVisibility::Visible);
		break;
	}
}

void UComponentAssemblyListItem::UpdateSelectionItemCount(int32 ItemCount)
{
	if (!ComponentPresetItem)
	{
		return;
	}
	if (ItemCount > 1)
	{
		ComponentPresetItem->MainText->ChangeText(FText::FromString(FString(TEXT("(")) + FString::FromInt(ItemCount) + FString(TEXT(") ")) + ItemDisplayName.ToString()));
	}
	else
	{
		ComponentPresetItem->MainText->ChangeText(ItemDisplayName);
	}
}

bool UComponentAssemblyListItem::BuildFromAssembly()
{
	if (!ComponentPresetItem || !BIMKey.IsValid())
	{
		return false;
	}
	ComponentPresetItem->CaptureIconFromPresetKey(EMPlayerController, BIMKey);

	TArray<FString> propertyTips;
	GetItemTips(propertyTips);

	for (auto curWidget : VerticalBoxProperties->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	VerticalBoxProperties->ClearChildren();

	for (auto &curTip : propertyTips)
	{
		UModumateTextBlockUserWidget *newModumateTextBlockUserWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UModumateTextBlockUserWidget>(ModumateTextBlockUserWidgetClass);
		if (newModumateTextBlockUserWidget)
		{
			newModumateTextBlockUserWidget->ChangeText(FText::FromString(curTip), true);
			VerticalBoxProperties->AddChildToVerticalBox(newModumateTextBlockUserWidget);
		}
	}
	return true;
}

void UComponentAssemblyListItem::OnModumateButtonMainReleased()
{
	if (ItemType == EComponentListItemType::AssemblyListItem  && EMPlayerController && EMPlayerController->EMPlayerState)
	{
		EMPlayerController->EMPlayerState->SetAssemblyForToolMode(ToolMode, BIMKey);
	}
}

void UComponentAssemblyListItem::OnButtonEditReleased()
{
	if (EMPlayerController)
	{
		EMPlayerController->EditModelUserWidget->EditExistingAssembly(ToolMode, BIMKey);
	}
}

void UComponentAssemblyListItem::OnButtonSwapReleased()
{
	if (EMPlayerController)
	{
		// Reset the search box in swap list
		EMPlayerController->EditModelUserWidget->SelectionTrayWidget->SelectionTray_Block_Swap->ResetSearchBox();

		EMPlayerController->EditModelUserWidget->SelectionTrayWidget->OpenToolTrayForSwap(ToolMode, BIMKey);
	}
}

void UComponentAssemblyListItem::OnButtonConfirmReleased()
{
	bool result = false;

	switch (ItemType)
	{
	case EComponentListItemType::SwapDesignerPreset:
		EMPlayerController->EditModelUserWidget->ToggleBIMPresetSwapTray(false);
		// This swap can be either from node, or from one the properties inside this node
		// If this item has a SwapScope and SwapNameType, then it is swapping a property
		// else is swapping the node
		if (SwapScope == EBIMValueScope::None && SwapNameType == NAME_None)
		{
			result = EMPlayerController->EditModelUserWidget->BIMDesigner->SetPresetForNodeInBIMDesigner(BIMInstanceID, BIMKey);
		}
		else
		{
			result = EMPlayerController->EditModelUserWidget->BIMDesigner->SetNodeProperty(BIMInstanceID, SwapScope, SwapNameType, BIMKey.ToString());
		}
		break;
	case EComponentListItemType::SwapListItem:
		UModumateDocument* doc = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
		const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByGUID(ToolMode, BIMKey);
		if (assembly)
		{
			TArray<int32> objIDs;
			for (auto& moi : EMPlayerController->EMPlayerState->SelectedObjects)
			{
				if (moi->GetAssembly().UniqueKey() == EMPlayerController->EditModelUserWidget->SelectionTrayWidget->GetCurrentPresetToSwap())
				{
					objIDs.Add(moi->ID);
				}
			}
			doc->SetAssemblyForObjects(GetWorld(), objIDs, *assembly);
			//Refresh swap menu
			EMPlayerController->EditModelUserWidget->SelectionTrayWidget->OpenToolTrayForSwap(ToolMode, BIMKey);
			result = true;
		}
		break;
	}
}

bool UComponentAssemblyListItem::GetItemTips(TArray<FString> &OutTips)
{
	if (!EMPlayerController)
	{
		return false;
	}

	UModumateDocument* doc = GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByGUID(ToolMode, BIMKey);
	if (!assembly)
	{
		return false;
	}

#if 0 // TODO: refactor for propertyless layers:
	FModumateFunctionParameterSet params;
	for (auto &curLayer : assembly->Layers)
	{
		// TODO: Only tips for wall tools for now. Will expand to other tool modes
		if (ToolMode == EToolMode::VE_WALL)
		{
			FString layerFunction, layerThickness;
			curLayer.Properties.TryGetProperty(EBIMValueScope::Layer, BIMPropertyNames::Function, layerFunction);
			curLayer.Properties.TryGetProperty(EBIMValueScope::Layer, BIMPropertyNames::Thickness, layerThickness);
			OutTips.Add(layerThickness + FString(TEXT(", ")) + layerFunction);
		}
	}
#endif
	return true;
}

void UComponentAssemblyListItem::SwitchToNormalStyle()
{
	ModumateButtonMain->SetStyle(NormalButtonStyle);
}

void UComponentAssemblyListItem::SwitchToActiveStyle()
{
	ModumateButtonMain->SetStyle(ActiveButtonStyle);
}

void UComponentAssemblyListItem::CheckIsCurrentToolAssemblyState()
{
	FGuid asmKey = EMPlayerController->EMPlayerState->GetAssemblyForToolMode(ToolMode);
	if (asmKey.IsValid() && asmKey == BIMKey)
	{
		SwitchToActiveStyle();
	}
	else
	{
		SwitchToNormalStyle();
	}
}

void UComponentAssemblyListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	UComponentListObject *compListObj = Cast<UComponentListObject>(ListItemObject);
	if (!compListObj)
	{
		return;
	}

	BIMKey = compListObj->UniqueKey;
	ToolMode = compListObj->Mode;
	SwapScope = compListObj->SwapScope;
	SwapNameType = compListObj->SwapNameType;
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController_CPP>();
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FPresetManager &presetManager = gameState->Document->PresetManager;
	
	// Find the preset for this list item. Note some item types do not require preset
	const FBIMPresetInstance* preset = presetManager.CraftingNodePresets.PresetFromGUID(BIMKey);

	switch (compListObj->ItemType)
	{
	case EComponentListItemType::SwapDesignerPreset:
		if (preset != nullptr)
		{
			ComponentPresetItem->MainText->ChangeText(preset->DisplayName);
		}
		BIMInstanceID = compListObj->BIMNodeInstanceID;
		ComponentPresetItem->CaptureIconForBIMDesignerSwap(EMPlayerController, compListObj->UniqueKey, BIMInstanceID);
		bIsNonAssemblyObjectSelectItem = false;
		UpdateItemType(compListObj->ItemType);
		return;

	case EComponentListItemType::AssemblyListItem:
	case EComponentListItemType::SwapListItem:
		if (preset == nullptr)
		{
			return;
		}
		ItemDisplayName = preset->DisplayName;
		ComponentPresetItem->MainText->ChangeText(ItemDisplayName);
		BuildFromAssembly();
		bIsNonAssemblyObjectSelectItem = false;
		UpdateItemType(compListObj->ItemType);
		if (compListObj->ItemType == EComponentListItemType::AssemblyListItem)
		{
			CheckIsCurrentToolAssemblyState();
		}
		return;

	case EComponentListItemType::SelectionListItem:
		if (preset == nullptr)
		{
			// Display the UniqueKey from compListObj if preset is missing
			// This can happen during selection of meta objects like edges and meta planes
			ItemDisplayName = FText::FromString(compListObj->UniqueKey.ToString());
			UpdateSelectionItemCount(compListObj->SelectionItemCount);
			bIsNonAssemblyObjectSelectItem = true;
			UpdateItemType(compListObj->ItemType);
			return;
		}
		ItemDisplayName = preset->DisplayName;
		UpdateSelectionItemCount(compListObj->SelectionItemCount);
		BuildFromAssembly();
		bIsNonAssemblyObjectSelectItem = false;
		UpdateItemType(compListObj->ItemType);
		return;
	}
}

