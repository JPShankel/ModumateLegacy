// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ComponentPresetListItem.h"


UComponentPresetListItem::UComponentPresetListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UComponentPresetListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UComponentPresetListItem::NativeConstruct()
{
	Super::NativeConstruct();
}