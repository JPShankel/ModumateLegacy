// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockSlotListItem.h"
#include "UI/Custom//ModumateTextBlock.h"
#include "UI/ModumateUIStatics.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"

UBIMBlockSlotListItem::UBIMBlockSlotListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockSlotListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBIMBlockSlotListItem::NativeConstruct()
{
	Super::NativeConstruct();
}
