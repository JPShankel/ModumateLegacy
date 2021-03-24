// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardObjectList.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/PresetCard/PresetCardMain.h"
#include "UI/Custom/ModumateButton.h"

#define LOCTEXT_NAMESPACE "PresetCardObjectList"

UPresetCardObjectList::UPresetCardObjectList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardObjectList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!MainButton)
	{
		return false;
	}

	MainButton->OnReleased.AddDynamic(this, &UPresetCardObjectList::OnMainButtonReleased);

	return true;
}

void UPresetCardObjectList::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardObjectList::OnMainButtonReleased()
{
	bool bExpandList = DynamicObjectList->GetChildrenCount() == 0;

	if (bIsDescendentlist)
	{
		BuildAsDescendentList(PresetGUID, bExpandList);
	}
	else
	{
		BuildAsAncestorList(PresetGUID, bExpandList);
	}
}

void UPresetCardObjectList::BuildAsDescendentList(const FGuid& InGUID, bool bAsExpandedList)
{
	EmptyList();
	bIsDescendentlist = true;
	PresetGUID = InGUID;

	TArray<FGuid> presetList;
	EMPlayerController->GetDocument()->GetPresetCollection().GetDescendentPresets(PresetGUID, presetList);

	MainText->ChangeText(FText::Format(
		LOCTEXT("DescendentsTitleFormat", "USES THESE PRESETS ({0})"),
		FText::AsNumber(presetList.Num())));

	if (bAsExpandedList)
	{
		for (auto& newPreset : presetList)
		{
			UPresetCardMain* newPresetWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardMain>(PresetCardMainClass);
			if (newPresetWidget)
			{
				DynamicObjectList->AddChildToVerticalBox(newPresetWidget);
				newPresetWidget->BuildAsCollapsedPresetCard(newPreset, false);
			}
		}
	}
}

void UPresetCardObjectList::BuildAsAncestorList(const FGuid& InGUID, bool bAsExpandedList)
{
	EmptyList();
	bIsDescendentlist = false;
	PresetGUID = InGUID;

	TArray<FGuid> presetList;
	EMPlayerController->GetDocument()->GetPresetCollection().GetAncestorPresets(PresetGUID, presetList);

	MainText->ChangeText(FText::Format(
		LOCTEXT("AncestorsTitleFormat", "USED BY PRESETS ({0})"),
		FText::AsNumber(presetList.Num())));

	if (bAsExpandedList)
	{
		for (auto& newPreset : presetList)
		{
			UPresetCardMain* newPresetWidget = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardMain>(PresetCardMainClass);
			if (newPresetWidget)
			{
				DynamicObjectList->AddChildToVerticalBox(newPresetWidget);
				newPresetWidget->BuildAsCollapsedPresetCard(newPreset, false);
			}
		}
	}
}

void UPresetCardObjectList::EmptyList()
{
	for (auto& curWidget : DynamicObjectList->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	DynamicObjectList->ClearChildren();
}

#undef LOCTEXT_NAMESPACE