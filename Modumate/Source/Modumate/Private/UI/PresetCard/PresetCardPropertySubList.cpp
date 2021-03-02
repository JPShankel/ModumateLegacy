// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardPropertySubList.h"

UPresetCardPropertySubList::UPresetCardPropertySubList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardPropertySubList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardPropertySubList::NativeConstruct()
{
	Super::NativeConstruct();
}
