// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Objects/ModumateObjectInstance.h"
#include "UI/ComponentAssemblyListItem.h"
#include "Database/ModumateObjectEnums.h"
#include "Components/ListView.h"
#include "UI/ComponentListObject.h"
#include "UI/PresetCard/PresetCardItemObject.h"


USelectionTrayBlockPresetList::USelectionTrayBlockPresetList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USelectionTrayBlockPresetList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	return true;
}

void USelectionTrayBlockPresetList::NativeConstruct()
{
	Super::NativeConstruct();
}

void USelectionTrayBlockPresetList::BuildPresetListFromSelection()
{
	ClearPresetList();
	TMap<FGuid, int32> numberOfObjectsWithKey;
	for (auto& curObject : Controller->EMPlayerState->SelectedObjects)
	{
		FGuid itemKey = curObject->GetAssembly().UniqueKey();
		// If GUID is invalid, use its object type to track its selection, ex: meta edges and planes
		if (!itemKey.IsValid())
		{
			FString objectTypeString = UModumateTypeStatics::GetTextForObjectType(curObject->GetObjectType()).ToString();
			FString objectTypeHash = FMD5::HashAnsiString(*objectTypeString);
			FGuid::Parse(objectTypeHash, itemKey);
		}
		UPresetCardItemObject* presetItem = PresetItemMap.FindRef(itemKey);
		if (presetItem)
		{
			int32 numberOfObj = numberOfObjectsWithKey.FindRef(itemKey);
			numberOfObjectsWithKey.Add(itemKey, numberOfObj + 1);
			presetItem->SelectionItemCount = numberOfObj + 1;

			UUserWidget* entryWidget = AssembliesList->GetEntryWidgetFromItem(presetItem);
			if (entryWidget)
			{
				UPresetCardMain* compListWidget = Cast<UPresetCardMain>(entryWidget);
				if (compListWidget)
				{
					compListWidget->UpdateSelectionItemCount(presetItem->SelectionItemCount);
				}
			}
		}
		else
		{
			UPresetCardItemObject* newPresetCardItemObj = NewObject<UPresetCardItemObject>(this);
			newPresetCardItemObj->PresetCardType = EPresetCardType::SelectTray;
			newPresetCardItemObj->ParentSelectionTrayBlockPresetList = this;
			newPresetCardItemObj->ObjectType = curObject->GetObjectType();
			newPresetCardItemObj->PresetGuid = itemKey;
			newPresetCardItemObj->SelectionItemCount = 1;
			AssembliesList->AddItem(newPresetCardItemObj);
			PresetItemMap.Add(itemKey, newPresetCardItemObj);
			numberOfObjectsWithKey.Add(itemKey, newPresetCardItemObj->SelectionItemCount);
		}
	}
}

void USelectionTrayBlockPresetList::ClearPresetList()
{
	PresetItemMap.Empty();
	AssembliesList->ClearListItems();
}

void USelectionTrayBlockPresetList::RefreshAssembliesListView()
{
	AssembliesList->RequestRefresh();
}
