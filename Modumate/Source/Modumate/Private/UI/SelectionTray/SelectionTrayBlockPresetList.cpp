// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Objects/ModumateObjectInstance.h"
#include "UI/ComponentAssemblyListItem.h"
#include "Database/ModumateObjectEnums.h"
#include "Components/ListView.h"
#include "UI/ComponentListObject.h"


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

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

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
#if 0 //GUIDTODO
		if (!itemKey.IsValid())
		{
			// If no assembly key is found, use its object type to track its selection, ex: meta edges and planes
			itemKey = UModumateTypeStatics::GetTextForObjectType(curObject->GetObjectType()).ToString();
		}
#endif
		UComponentListObject* compItem = ComponentItemMap.FindRef(itemKey);
		if (compItem)
		{
			int32 numberOfObj = numberOfObjectsWithKey.FindRef(itemKey);
			numberOfObjectsWithKey.Add(itemKey, numberOfObj + 1);
			compItem->SelectionItemCount = numberOfObj + 1;

			UUserWidget* entryWidget = AssembliesList->GetEntryWidgetFromItem(compItem);
			if (entryWidget)
			{
				UComponentAssemblyListItem* compListWidget = Cast<UComponentAssemblyListItem>(entryWidget);
				if (compListWidget)
				{
					compListWidget->UpdateSelectionItemCount(compItem->SelectionItemCount);
				}
			}
		}
		else
		{
			UComponentListObject* newCompListObj = NewObject<UComponentListObject>(this);
			newCompListObj->ItemType = EComponentListItemType::SelectionListItem;
			newCompListObj->Mode = UModumateTypeStatics::ToolModeFromObjectType(curObject->GetObjectType());
			newCompListObj->UniqueKey = itemKey;
			newCompListObj->SelectionItemCount = 1;
			AssembliesList->AddItem(newCompListObj);
			ComponentItemMap.Add(itemKey, newCompListObj);
			numberOfObjectsWithKey.Add(itemKey, newCompListObj->SelectionItemCount);
		}
	}
}

void USelectionTrayBlockPresetList::ClearPresetList()
{
	ComponentItemMap.Empty();
	AssembliesList->ClearListItems();
}
