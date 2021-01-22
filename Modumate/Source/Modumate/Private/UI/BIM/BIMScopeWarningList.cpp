// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMScopeWarningList.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/BIM/BIMScopeWarningListItem.h"



UBIMScopeWarningList::UBIMScopeWarningList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMScopeWarningList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBIMScopeWarningList::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMScopeWarningList::AddItemToVerticalList(class UBIMScopeWarningListItem* ItemToAdd, const FMargin& ItemPadding)
{
	PresetItemList->AddChildToVerticalBox(ItemToAdd);
	int32 numChildren = PresetItemList->GetAllChildren().Num();
	FString newListTitle = FString(TEXT("(")) + FString::FromInt(numChildren) + FString(TEXT(") ")) + CurrentDefinitionName;
	TypeText->ChangeText(FText::FromString(newListTitle));

	// Padding for item widget
	UVerticalBoxSlot* itemSlot = UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(ItemToAdd);
	if (itemSlot)
	{
		itemSlot->SetPadding(ItemPadding);
	}
}

void UBIMScopeWarningList::BuildPresetListTitle(const FString& DefinitionName)
{
	CurrentDefinitionName = DefinitionName;
	TypeText->ChangeText(FText::FromString(CurrentDefinitionName));
}
