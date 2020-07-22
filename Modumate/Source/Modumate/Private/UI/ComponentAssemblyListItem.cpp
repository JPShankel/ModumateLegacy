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

	if (!(ModumateButtonMain && ButtonEdit))
	{
		return false;
	}

	ModumateButtonMain->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnModumateButtonMainReleased);
	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UComponentAssemblyListItem::OnButtonEditReleased);

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
	if (EMPlayerController && EMPlayerController->EMPlayerState)
	{
		EMPlayerController->EMPlayerState->SetAssemblyForToolMode(ToolMode, AsmKey);
	}
}

void UComponentAssemblyListItem::OnButtonEditReleased()
{
	if (EMPlayerController)
	{
		EMPlayerController->EditModelUserWidget->EventEditExistingAssembly(ToolMode, AsmKey);
	}
}

bool UComponentAssemblyListItem::GetItemTips(TArray<FString> &OutTips)
{
	if (!EMPlayerController)
	{
		return false;
	}
	FModumateDocument *doc = &GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;
	const FModumateObjectAssembly *assembly = doc->PresetManager.GetAssemblyByKey(ToolMode, AsmKey);
	if (!assembly)
	{
		return false;
	}

	// Get properties for tips
	BIM::FModumateAssemblyPropertySpec presetSpec;
	doc->PresetManager.PresetToSpec(assembly->RootPreset, presetSpec);

	FModumateFunctionParameterSet params;
	for (auto &curLayerProperties : presetSpec.LayerProperties)
	{
		// TODO: Only tips for wall tools for now. Will expand to other tool modes
		if (ToolMode == EToolMode::VE_WALL)
		{
			FString layerFunction, layerThickness;
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Function, layerFunction);
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Thickness, layerThickness);
			OutTips.Add(layerThickness + FString(TEXT(", ")) + layerFunction);
		}

	}
	return true;
}

void UComponentAssemblyListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	UComponentListObject *compListObj = Cast<UComponentListObject>(ListItemObject);
	if (!compListObj)
	{
		return;
	}

	EMPlayerController = GetOwningPlayer<AEditModelPlayerController_CPP>();
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FPresetManager &presetManager = gameState->Document.PresetManager;

	AsmKey = compListObj->UniqueKey;
	const FModumateObjectAssembly *assembly = presetManager.GetAssemblyByKey(AsmKey);
	if (!assembly)
	{
		return;
	}
	AsmName = assembly->GetProperty(BIM::Parameters::Name);
	ToolMode = compListObj->Mode;
	UpdateItemType(compListObj->ItemType);

	switch (ItemType)
	{
	case EComponentListItemType::AssemblyListItem:
		ComponentPresetItem->MainText->ChangeText(FText::FromName(AsmName));
		BuildFromAssembly();
		break;
	case EComponentListItemType::SelectionListItem:
		UpdateSelectionItemCount(compListObj->SelectionItemCount);
		BuildFromAssembly();
		break;
	}
}

