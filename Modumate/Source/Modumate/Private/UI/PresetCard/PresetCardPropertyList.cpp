// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardPropertyList.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/PresetCard/PresetCardPropertyText.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateButton.h"

UPresetCardPropertyList::UPresetCardPropertyList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardPropertyList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!MainButton)
	{
		return false;
	}

	MainButton->OnReleased.AddDynamic(this, &UPresetCardPropertyList::OnMainButtonReleased);

	return true;
}

void UPresetCardPropertyList::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardPropertyList::OnMainButtonReleased()
{
	bool bExpandList = DynamicPropertyList->GetChildrenCount() == 0;
	BuildAsPropertyList(PresetGUID, bExpandList);
}

void UPresetCardPropertyList::BuildAsPropertyList(const FGuid& InGUID, bool bAsExpandedList)
{
	EmptyList();
	PresetGUID = InGUID;

	if (bAsExpandedList)
	{
		const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(InGUID);
		if (preset)
		{
			FBIMPresetForm presetForm;
			preset->GetForm(presetForm);
			for (auto& curElem : presetForm.Elements)
			{
				curElem.DisplayName;
				curElem.StringRepresentation;

				UPresetCardPropertyText* newText = EMPlayerController->GetEditModelHUD()->GetOrCreateWidgetInstance<UPresetCardPropertyText>(PresetCardPropertyTextClass);
				if (newText)
				{
					newText->TextTitle->ChangeText(curElem.DisplayName);
					newText->TextValue->ChangeText(FText::FromString(curElem.StringRepresentation));
					DynamicPropertyList->AddChildToVerticalBox(newText);
				}
			}
		}
	}
}

void UPresetCardPropertyList::EmptyList()
{
	for (auto& curWidget : DynamicPropertyList->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			EMPlayerController->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	DynamicPropertyList->ClearChildren();
}
