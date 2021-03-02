// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardPropertyList.h"

UPresetCardPropertyList::UPresetCardPropertyList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardPropertyList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardPropertyList::NativeConstruct()
{
	Super::NativeConstruct();
}
