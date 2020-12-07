// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Debugger/BIMDebugPresetListItem.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Debugger/BIMDebugPresetListItemObj.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/Debugger/BIMDebugger.h"


UBIMDebugPresetListItem::UBIMDebugPresetListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMDebugPresetListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (ModumateButtonMain)
	{
		ModumateButtonMain->OnReleased.AddDynamic(this, &UBIMDebugPresetListItem::OnButtonMainReleased);
	}

	return true;
}

void UBIMDebugPresetListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMDebugPresetListItem::OnButtonMainReleased()
{
	ParentDebugger->DebugBIMPreset(PresetKey, !bIsFromHistoryMenu);
}
void UBIMDebugPresetListItem::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UBIMDebugPresetListItemObj* presetListItemObj = Cast<UBIMDebugPresetListItemObj>(ListItemObject);
	if (!presetListItemObj)
	{
		return;
	}

	PresetKey = presetListItemObj->PresetKey;
	MainText->SetText(presetListItemObj->DisplayName);
	ParentDebugger = presetListItemObj->ParentDebugger;
	bIsFromHistoryMenu = presetListItemObj->bIsFromHistoryMenu;
	SetToolTipText(FText::FromString(PresetKey.ToString()));
	ModumateButtonMain->SetIsEnabled(presetListItemObj->bItemIsPreset);
}

