// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentAssemblyListItem.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
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
#include "UnrealClasses/EditModelPlayerState.h"
#include "Components/Image.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "Database/ModumateObjectEnums.h"

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

	if (!(ModumateButtonMain && ButtonEdit && ButtonSwap && ButtonConfirm && ButtonDuplicate))
	{
		return false;
	}

	ModumateButtonMain->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnModumateButtonMainReleased);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonEditReleased);
	ButtonSwap->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonSwapReleased);
	ButtonConfirm->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonConfirmReleased);
	ButtonDuplicate->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonDuplicateReleased);

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
		ButtonDuplicate->SetVisibility(ESlateVisibility::Visible);
		break;
	case EComponentListItemType::SelectionListItem:
		ButtonSwap->SetVisibility(bIsNonAssemblyObjectSelectItem ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(bIsNonAssemblyObjectSelectItem ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
		ButtonDuplicate->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EComponentListItemType::SwapListItem:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Collapsed);
		ButtonConfirm->SetVisibility(ESlateVisibility::Visible);
		ButtonDuplicate->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EComponentListItemType::SwapDesignerPreset:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Collapsed);
		ButtonConfirm->SetVisibility(ESlateVisibility::Visible);
		ButtonDuplicate->SetVisibility(ESlateVisibility::Visible);
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

FGuid UComponentAssemblyListItem::DuplicatePreset() const
{
	UModumateDocument* doc = GetWorld()->GetGameState<AEditModelGameState>()->Document;
	FBIMPresetInstance newPreset;
	auto delta = doc->GetPresetCollection().MakeDuplicateDelta(BIMKey, newPreset);
	if (ensureAlways(delta != nullptr))
	{
		ensureAlways(doc->ApplyPresetDelta(*delta, GetWorld()));
		return newPreset.GUID;
	}
	return FGuid();
}

void UComponentAssemblyListItem::OnButtonDuplicateReleased()
{
	switch (ItemType)
	{
	case EComponentListItemType::AssemblyListItem:
	{
		FGuid duplicate = DuplicatePreset();
		if (ensureAlways(duplicate.IsValid() && EMPlayerController))
		{
			EMPlayerController->EditModelUserWidget->EditExistingAssembly(duplicate);
		}
	}
	break;

	case EComponentListItemType::SwapDesignerPreset:
	{
		FGuid duplicate = DuplicatePreset();
		EMPlayerController->EditModelUserWidget->ToggleBIMPresetSwapTray(false);
		if (ensureAlways(duplicate.IsValid()))
		{
			if (FormElement.FieldType == EBIMPresetEditorField::None)
			{
				EMPlayerController->EditModelUserWidget->BIMDesigner->SetPresetForNodeInBIMDesigner(BIMInstanceID, duplicate);
			}
			else
			{
				FormElement.StringRepresentation = BIMKey.ToString();
				EMPlayerController->EditModelUserWidget->BIMDesigner->ApplyBIMFormElement(BIMInstanceID, FormElement);
			}
		}
	}
	break;
	};
}

void UComponentAssemblyListItem::OnButtonEditReleased()
{
	if (EMPlayerController)
	{
		EMPlayerController->EditModelUserWidget->EditExistingAssembly(BIMKey);
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
		// If the form item is uninitialized, then we're swapping the whole preset, otherwise it's a property or custom data field
		if (FormElement.FieldType == EBIMPresetEditorField::None)
		{
			result = EMPlayerController->EditModelUserWidget->BIMDesigner->SetPresetForNodeInBIMDesigner(BIMInstanceID, BIMKey);
		}
		else
		{
			FormElement.StringRepresentation = BIMKey.ToString();
			result = EMPlayerController->EditModelUserWidget->BIMDesigner->ApplyBIMFormElement(BIMInstanceID, FormElement);
		}
		break;
	case EComponentListItemType::SwapListItem:
		UModumateDocument* doc = GetWorld()->GetGameState<AEditModelGameState>()->Document;
		const FBIMAssemblySpec *assembly = doc->GetPresetCollection().GetAssemblyByGUID(ToolMode, BIMKey);
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

	UModumateDocument* doc = GetWorld()->GetGameState<AEditModelGameState>()->Document;
	const FBIMAssemblySpec *assembly = doc->GetPresetCollection().GetAssemblyByGUID(ToolMode, BIMKey);
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
	FormElement = compListObj->FormElement;
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	
	// Find the preset for this list item. Note some item types do not require preset
	const FBIMPresetInstance* preset = gameState->Document->GetPresetCollection().PresetFromGUID(BIMKey);

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
			FString objectTypeString = UModumateTypeStatics::GetTextForObjectType(compListObj->ObjType).ToString();
			ItemDisplayName = FText::FromString(objectTypeString);
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

