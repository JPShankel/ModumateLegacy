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
}

void UComponentAssemblyListItem::UpdateItemType(EComponentListItemType NewItemType)
{
	ItemType = NewItemType;
	if (!(ButtonEdit && ButtonSwap && ButtonTrash && ButtonConfirm))
	{
		return;
	}
	switch (ItemType)
	{
	case EComponentListItemType::AssemblyListItem:
		ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
		ButtonTrash->SetVisibility(ESlateVisibility::Visible);
		ButtonEdit->SetVisibility(ESlateVisibility::Visible);
		ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
		break;
	case EComponentListItemType::SelectionListItem:
		ButtonSwap->SetVisibility(ESlateVisibility::Visible);
		ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
		ButtonEdit->SetVisibility(ESlateVisibility::Visible);
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
		ComponentPresetItem->MainText->ChangeText(FText::FromString(FString(TEXT("(")) + FString::FromInt(ItemCount) + FString(TEXT(") ")) + AsmName.ToString()));
	}
	else
	{
		ComponentPresetItem->MainText->ChangeText(FText::FromName(AsmName));
	}
}

bool UComponentAssemblyListItem::BuildFromAssembly()
{
	if (!ComponentPresetItem || AsmKey.IsNone())
	{
		return false;
	}
	ComponentPresetItem->CaptureIconFromPresetKey(EMPlayerController, AsmKey, ToolMode);

	TArray<FString> propertyTips;
	GetItemTips(propertyTips);
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
		EMPlayerController->EMPlayerState->SetAssemblyForToolMode(ToolMode, AsmKey);
	}
}

void UComponentAssemblyListItem::OnButtonEditReleased()
{
	if (EMPlayerController)
	{
		EMPlayerController->EditModelUserWidget->EditExistingAssembly(ToolMode, AsmKey);
	}
}

void UComponentAssemblyListItem::OnButtonSwapReleased()
{
	if (EMPlayerController)
	{
		EMPlayerController->EditModelUserWidget->SelectionTrayWidget->OpenToolTrayForSwap(ToolMode, AsmKey);
	}
}

void UComponentAssemblyListItem::OnButtonConfirmReleased()
{
	bool result = false;

	switch (ItemType)
	{
	case EComponentListItemType::SwapDesignerPreset:
		result = EMPlayerController->EditModelUserWidget->BIMDesigner->SetPresetForNodeInBIMDesigner(BIMInstanceID, AsmKey);
		break;
	case EComponentListItemType::SwapListItem:
		FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
		const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByKey(ToolMode, AsmKey);
		if (assembly)
		{
			TArray<int32> objIDs;
			TArray<FModumateObjectInstance*> mois = EMPlayerController->EMPlayerState->SelectedObjects;
			for (auto& moi : mois)
			{
				if (moi->GetAssembly().UniqueKey() == EMPlayerController->EditModelUserWidget->SelectionTrayWidget->GetCurrentPresetToSwap())
				{
					objIDs.Add(moi->ID);
				}
			}
			doc->SetAssemblyForObjects(GetWorld(), objIDs, *assembly);
			//Refresh swap menu
			EMPlayerController->EditModelUserWidget->SelectionTrayWidget->OpenToolTrayForSwap(ToolMode, AsmKey);
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

	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	const FBIMAssemblySpec *assembly = doc->PresetManager.GetAssemblyByKey(ToolMode, AsmKey);
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

void UComponentAssemblyListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	UComponentListObject *compListObj = Cast<UComponentListObject>(ListItemObject);
	if (!compListObj)
	{
		return;
	}

	UpdateItemType(compListObj->ItemType);
	AsmKey = compListObj->UniqueKey;
	ToolMode = compListObj->Mode;
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController_CPP>();
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FPresetManager &presetManager = gameState->Document.PresetManager;

	// Swappable preset item doesn't have BIMAssemblySpec yet.
	if (compListObj->ItemType == EComponentListItemType::SwapDesignerPreset)
	{
		// TODO: need human readable assembly name
		ComponentPresetItem->MainText->ChangeText(FText::FromString(compListObj->UniqueKey.ToString()));
		BIMInstanceID = compListObj->BIMNodeInstanceID;
		// TODO: swap item icons shouldn't take dirty values from children, make BIMInstanceID = none?
		ComponentPresetItem->CaptureIconForBIMDesignerSwap(EMPlayerController, compListObj->UniqueKey, BIMInstanceID);
		return;
	}

	const FBIMAssemblySpec *assembly = presetManager.GetAssemblyByKey(compListObj->Mode,AsmKey);
	if (!assembly)
	{
		return;
	}
	AsmName = *(assembly->DisplayName);

	switch (ItemType)
	{
	case EComponentListItemType::AssemblyListItem:
	case EComponentListItemType::SwapListItem:
		ComponentPresetItem->MainText->ChangeText(FText::FromName(AsmName));
		BuildFromAssembly();
		break;
	case EComponentListItemType::SelectionListItem:
		UpdateSelectionItemCount(compListObj->SelectionItemCount);
		BuildFromAssembly();
		break;
	}
}

