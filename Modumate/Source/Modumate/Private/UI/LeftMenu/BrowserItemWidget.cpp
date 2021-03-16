// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/LeftMenu/BrowserItemWidget.h"
#include "UI/LeftMenu/BrowserItemObj.h"
#include "UI/PresetCard/PresetCardMain.h"
#include "UI/LeftMenu/NCPButton.h"

UBrowserItemWidget::UBrowserItemWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBrowserItemWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBrowserItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBrowserItemWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	UBrowserItemObj* itemObj = Cast<UBrowserItemObj>(ListItemObject);
	if (!itemObj)
	{
		return;
	}

	if (itemObj->bAsPresetCard)
	{
		PresetCardMainWidget->SetVisibility(ESlateVisibility::Visible);
		NCPButtonWidget->SetVisibility(ESlateVisibility::Collapsed);
		PresetCardMainWidget->SetParentWidgets(itemObj->ParentNCPNavigator, itemObj);
		if (itemObj->bPresetCardExpanded)
		{
			PresetCardMainWidget->BuildAsBrowserSelectedPresetCard(itemObj->PresetGuid);
		}
		else
		{
			PresetCardMainWidget->BuildAsBrowserCollapsedPresetCard(itemObj->PresetGuid, true);
		}
	}
	else
	{
		PresetCardMainWidget->SetVisibility(ESlateVisibility::Collapsed);
		NCPButtonWidget->SetVisibility(ESlateVisibility::Visible);
		NCPButtonWidget->BuildButton(itemObj->ParentNCPNavigator, itemObj->NCPTag, itemObj->TagOrder, itemObj->bNCPButtonExpanded);
	}
}
