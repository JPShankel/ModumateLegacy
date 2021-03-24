// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardQuantityListSubTotalListItem.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"

#define LOCTEXT_NAMESPACE "PresetCardQuantityListSubTotalListItem"

UPresetCardQuantityListSubTotalListItem::UPresetCardQuantityListSubTotalListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardQuantityListSubTotalListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UPresetCardQuantityListSubTotalListItem::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardQuantityListSubTotalListItem::BuildAsSubTotalListItem(const FQuantityItemId& QuantityItemID, const FQuantity& InQuantity)
{
	const FBIMPresetInstance* curPreset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(QuantityItemID.Id);
	if (curPreset)
	{
		FText displayName = FText::Format(LOCTEXT("SubtotalFormat", "{0} {1}"), curPreset->DisplayName, FText::FromString(QuantityItemID.Subname));
		AssemblyTitle->ChangeText(displayName);
	}

	if (InQuantity.Count > 0)
	{
		Quantity1->ChangeText(FText::AsNumber(InQuantity.Count));
		Quantity2->ChangeText(FText::GetEmpty());
	}
	else
	{
		Quantity1->ChangeText(FText::AsNumber(InQuantity.Area));
		Quantity2->ChangeText(FText::AsNumber(InQuantity.Linear));
	}
}

#undef LOCTEXT_NAMESPACE