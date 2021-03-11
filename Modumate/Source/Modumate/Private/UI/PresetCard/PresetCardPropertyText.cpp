// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardPropertyText.h"

UPresetCardPropertyText::UPresetCardPropertyText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardPropertyText::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardPropertyText::NativeConstruct()
{
	Super::NativeConstruct();
}