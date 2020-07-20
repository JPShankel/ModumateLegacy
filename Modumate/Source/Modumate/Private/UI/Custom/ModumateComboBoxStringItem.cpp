// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Custom/ModumateComboBoxStringItem.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "Components/Image.h"


UModumateComboBoxStringItem::UModumateComboBoxStringItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModumateComboBoxStringItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	return true;
}

void UModumateComboBoxStringItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UModumateComboBoxStringItem::BuildItem(FText NewText, UTexture2D *NewTexture)
{
	ModumateTextBlockUserWidget->ChangeText(NewText);
	if (NewTexture)
	{
		ImageBlock->SetBrushFromTexture(NewTexture);
	}
}
