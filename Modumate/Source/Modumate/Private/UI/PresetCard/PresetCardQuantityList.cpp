// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardQuantityList.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateButton.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Quantities/QuantitiesManager.h"
#include "UI/PresetCard/PresetCardQuantityListTotal.h"
#include "UI/PresetCard/PresetCardQuantityListSubTotal.h"
#include "UI/PresetCard/PresetCardQuantityListSubTotalListItem.h"

#define LOCTEXT_NAMESPACE "PresetCardQuantityList"

UPresetCardQuantityList::UPresetCardQuantityList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardQuantityList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!MainButton)
	{
		return false;
	}

	MainButton->OnReleased.AddDynamic(this, &UPresetCardQuantityList::OnMainButtonReleased);

	return true;
}

void UPresetCardQuantityList::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardQuantityList::OnMainButtonReleased()
{
	bool bExpandList = DynamicQuantityList->GetChildrenCount() == 0;
	BuildAsQuantityList(PresetGUID, bExpandList);
}

void UPresetCardQuantityList::BuildAsQuantityList(const FGuid& InGUID, bool bAsExpandedList)
{
	// deprecated, moved to web
}

void UPresetCardQuantityList::EmptyList()
{
	for (auto& curWidget : DynamicQuantityList->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	DynamicQuantityList->ClearChildren();
}

#undef LOCTEXT_NAMESPACE
