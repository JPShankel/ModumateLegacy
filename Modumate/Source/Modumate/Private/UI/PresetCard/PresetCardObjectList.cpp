// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardObjectList.h"

UPresetCardObjectList::UPresetCardObjectList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardObjectList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardObjectList::NativeConstruct()
{
	Super::NativeConstruct();
}
