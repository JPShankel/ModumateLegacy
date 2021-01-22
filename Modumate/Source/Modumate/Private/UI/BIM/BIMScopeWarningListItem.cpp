// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMScopeWarningListItem.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"


UBIMScopeWarningListItem::UBIMScopeWarningListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMScopeWarningListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBIMScopeWarningListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMScopeWarningListItem::BuildItem(const FString& ItemName)
{

	PresetTitle->ChangeText(FText::FromString(ItemName));
}
