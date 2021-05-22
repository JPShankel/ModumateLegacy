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
		PresetCardMainWidget->SetAsNCPNavigatorPresetCard(itemObj->ParentNCPNavigator, itemObj, itemObj->PresetCardType);
		
		// AssemblyList only allows one preset card selected as current tool assembly
		if (itemObj->PresetCardType == EPresetCardType::AssembliesList)
		{
			PresetCardMainWidget->BuildAsAssemblyListPresetCard(itemObj);
		}
		else if (itemObj->bPresetCardExpanded)
		{
			PresetCardMainWidget->BuildAsExpandedPresetCard(itemObj->PresetGuid);
		}
		else
		{
			PresetCardMainWidget->BuildAsCollapsedPresetCard(itemObj->PresetGuid, true);
		}
	}
	else
	{
		PresetCardMainWidget->SetVisibility(ESlateVisibility::Collapsed);
		NCPButtonWidget->SetVisibility(ESlateVisibility::Visible);
		NCPButtonWidget->BuildButton(itemObj->ParentNCPNavigator, itemObj->NCPTag, itemObj->TagOrder, itemObj->bNCPButtonExpanded);
	}
}
