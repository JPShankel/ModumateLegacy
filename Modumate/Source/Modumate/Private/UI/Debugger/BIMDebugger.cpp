// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/Debugger/BIMDebugger.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "Components/ListView.h"
#include "UI/Debugger/BIMDebugPresetListItemObj.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "Components/EditableText.h"
#include "Components/MultiLineEditableText.h"
#include "Kismet/KismetStringLibrary.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/Debugger/BIMDebugTestITem.h"
#include "Components/VerticalBox.h"

UBIMDebugger::UBIMDebugger(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMDebugger::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!(ButtonUpdatePresetList && SearchField && ButtonClearHistory && ButtonClose))
	{
		return false;
	}

	ButtonUpdatePresetList->OnReleased.AddDynamic(this, &UBIMDebugger::OnButtonUpdatePresetListReleased);
	ButtonClearHistory->OnReleased.AddDynamic(this, &UBIMDebugger::ClearPresetHistory);
	ButtonClose->OnReleased.AddDynamic(this, &UBIMDebugger::OnButtonCloseReleased);

	if (SearchField)
	{
		SearchField->OnTextChanged.AddDynamic(this, &UBIMDebugger::OnSearchFieldChanged);
	}

	return true;
}

void UBIMDebugger::NativeConstruct()
{

}

void UBIMDebugger::OnButtonUpdatePresetListReleased()
{
	ConstructPresetList();
}

void UBIMDebugger::ClearPresetHistory()
{
	PresetHistoryList->ClearListItems();
	LastSelectedKey = FGuid();
}

void UBIMDebugger::OnButtonCloseReleased()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBIMDebugger::OnSearchFieldChanged(const FText& NewText)
{
	ConstructPresetList();
}

EBIMResult UBIMDebugger::ConstructPresetList()
{
	class AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (!controller)
	{
		return EBIMResult::Error;
	}

	PresetList->ClearListItems();
	PresetList->SetScrollOffset(0.f);

	for (auto& kvp : controller->GetDocument()->PresetManager.CraftingNodePresets.PresetsByGUID)
	{
		if (IsPresetAvailableForSearch(kvp.Value))
		{
			UBIMDebugPresetListItemObj* newPresetObj = NewObject<UBIMDebugPresetListItemObj>(this);
			newPresetObj->PresetKey = kvp.Value.GUID;
			newPresetObj->DisplayName = kvp.Value.DisplayName;
			newPresetObj->ParentDebugger = this;
			newPresetObj->bItemIsPreset = true;
			newPresetObj->bIsFromHistoryMenu = false;
			PresetList->AddItem(newPresetObj);
		}
	}

	return EBIMResult::Success;
}

EBIMResult UBIMDebugger::DebugBIMPreset(const FGuid& PresetKey, bool AddToHistory)
{
	DependentPresetsList->ClearListItems();
	BIMDebugTestList->ClearChildren();

	class AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller)
	{
		const FBIMPresetInstance* preset = controller->GetDocument()->PresetManager.CraftingNodePresets.PresetFromGUID(PresetKey);
		if (preset)
		{
			TextDisplayName->SetText(preset->DisplayName);
			TextGUID->SetText(FText::FromString(preset->GUID.ToString()));
			TextBIMKey->SetText(FText::FromString(PresetKey.ToString()));

			TMap<FString, FBIMNameType> properties;
			controller->GetDocument()->PresetManager.CraftingNodePresets.GetPropertyFormForPreset(PresetKey, properties);

			for (auto& kvp : properties)
			{
				UBIMDebugPresetListItemObj* newPresetObj = NewObject<UBIMDebugPresetListItemObj>(this);

				// Get this property scope, name, and value
				FBIMPropertyKey propertyValue(kvp.Value);
				FGuid propertyPresetKey;
				Modumate::Units::FUnitValue unitValue;
				bool bPropertyIsPreset = preset->Properties.TryGetProperty(propertyValue.Scope, propertyValue.Name, propertyPresetKey);
				if (!bPropertyIsPreset)
				{
					preset->Properties.TryGetProperty(propertyValue.Scope, propertyValue.Name, unitValue);
				}

				// Convert value into string
				FString stringValue;
				if (bPropertyIsPreset)
				{
					stringValue = propertyPresetKey.ToString();
				}
				else
				{
					TArray<int32> imperialsInches;
					UModumateDimensionStatics::CentimetersToImperialInches(unitValue.AsWorldCentimeters(), imperialsInches);
					stringValue = UModumateDimensionStatics::ImperialInchesToDimensionStringText(imperialsInches).ToString();
				}

				// Build new item
				FString itemNameString = propertyValue.QN().ToString() + FString::Printf(TEXT(": ")) + stringValue;
				newPresetObj->DisplayName = FText::FromString(itemNameString);
				newPresetObj->PresetKey = propertyPresetKey;
				newPresetObj->bItemIsPreset = bPropertyIsPreset;
				newPresetObj->bIsFromHistoryMenu = false;
				newPresetObj->ParentDebugger = this;
				DependentPresetsList->AddItem(newPresetObj);
			}

			// Add other info
			FString tagPathString;
			preset->MyTagPath.ToString(tagPathString);

			FString otherDebugString =
				FString::Printf(TEXT("SlotConfigPresetID: ")) + preset->SlotConfigPresetGUID.ToString() + LINE_TERMINATOR
				+ FString::Printf(TEXT("CategoryTitle: ")) + preset->CategoryTitle.ToString() + LINE_TERMINATOR
				+ FString::Printf(TEXT("MyTagPath: ")) + tagPathString + LINE_TERMINATOR;

			TextOtherInfo->SetText(FText::FromString(otherDebugString));

			// Add to history
			if (AddToHistory && LastSelectedKey != PresetKey)
			{
				LastSelectedKey = PresetKey;
				UBIMDebugPresetListItemObj* newPresetObj = NewObject<UBIMDebugPresetListItemObj>(this);
				newPresetObj->PresetKey = PresetKey;
				newPresetObj->DisplayName = preset->DisplayName;
				newPresetObj->ParentDebugger = this;
				newPresetObj->bItemIsPreset = true;
				newPresetObj->bIsFromHistoryMenu = true;
				PresetHistoryList->AddItem(newPresetObj);
			}

			ConstructBIMDebugTestList();

			return EBIMResult::Success;
		}
	}

	FString errorString = FString::Printf(TEXT("Error, can't find ")) + PresetKey.ToString();
	TextDisplayName->SetText(FText::GetEmpty());
	TextGUID->SetText(FText::GetEmpty());
	TextBIMKey->SetText(FText::FromString(errorString));

	return EBIMResult::Error;
}

bool UBIMDebugger::IsPresetAvailableForSearch(const FBIMPresetInstance& SearchPreset)
{
	FString searchSubString = SearchField->GetText().ToString();
	if (searchSubString.Len() == 0)
	{
		return true;
	}

	if (UKismetStringLibrary::Contains(SearchPreset.DisplayName.ToString(), searchSubString))
	{
		return true;
	}

	if (UKismetStringLibrary::Contains(SearchPreset.GUID.ToString(), searchSubString))
	{
		return true;
	}

	if (UKismetStringLibrary::Contains(SearchPreset.PresetID.ToString(), searchSubString))
	{
		return true;
	}

	return false;
}

void UBIMDebugger::ConstructBIMDebugTestList()
{
	BIMDebugTestList->ClearChildren();

	for (EBIMDebugTestType curType : TEnumRange<EBIMDebugTestType>())
	{
		UBIMDebugTestItem* newValidatePresetItem = CreateWidget<UBIMDebugTestItem>(this, BIMDebugTestItemClass);
		newValidatePresetItem->BuildDebugTestItem(this, curType);
		BIMDebugTestList->AddChildToVerticalBox(newValidatePresetItem);
	}
}

bool UBIMDebugger::PerformBIMDebugTest(EBIMDebugTestType BIMDebugTest)
{
	class AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	const FBIMPresetInstance* preset = controller ? controller->GetDocument()->PresetManager.CraftingNodePresets.PresetFromGUID(LastSelectedKey) : nullptr;
	if (!preset)
	{
		return false;
	}

	switch (BIMDebugTest)
	{
	case EBIMDebugTestType::TestDebugFalse:
		return false;
	case EBIMDebugTestType::TestDebugTrue:
		return true;
	case EBIMDebugTestType::ValidatePreset:
		return preset->ValidatePreset();
	}

	return false;
}
