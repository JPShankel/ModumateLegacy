// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardMain.h"
#include "UI/Custom/ModumateButton.h"

UPresetCardMain::UPresetCardMain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardMain::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!MainButton)
	{
		return false;
	}

	MainButton->OnReleased.AddDynamic(this, &UPresetCardMain::OnMainButtonReleased);

	return true;
}

void UPresetCardMain::NativeConstruct()
{
	Super::NativeConstruct();
}

void UPresetCardMain::OnMainButtonReleased()
{

}
