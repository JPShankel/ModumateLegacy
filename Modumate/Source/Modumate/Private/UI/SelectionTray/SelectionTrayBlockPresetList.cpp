// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateObjectInstance.h"
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
	TMap<FName, int32> numberOfObjectsWithKey;
	for (auto& curObject : Controller->EMPlayerState->SelectedObjects)
	{
		if (curObject->GetActor())
		{
			UComponentListObject *compItem = ComponentItemMap.FindRef(curObject->GetAssembly().DatabaseKey);
			if (compItem)
			{
				int32 numberOfObj = numberOfObjectsWithKey.FindRef(curObject->GetAssembly().DatabaseKey);
				numberOfObjectsWithKey.Add(curObject->GetAssembly().DatabaseKey, numberOfObj + 1);
				compItem->SelectionItemCount = numberOfObj + 1;

				UUserWidget *entryWidget = AssembliesList->GetEntryWidgetFromItem(compItem);
				if (entryWidget)
				{
					UComponentAssemblyListItem *compListWidget = Cast<UComponentAssemblyListItem>(entryWidget);
					if (compListWidget)
					{
						compListWidget->UpdateSelectionItemCount(compItem->SelectionItemCount);
					}
				}
			}
			else
			{
				UComponentListObject *newCompListObj = NewObject<UComponentListObject>(this);
				newCompListObj->ItemType = EComponentListItemType::SelectionListItem;
				newCompListObj->Mode = UModumateTypeStatics::ToolModeFromObjectType(curObject->GetObjectType());
				newCompListObj->UniqueKey = curObject->GetAssembly().DatabaseKey;
				newCompListObj->SelectionItemCount = 1;
				AssembliesList->AddItem(newCompListObj);
				ComponentItemMap.Add(curObject->GetAssembly().DatabaseKey, newCompListObj);
				numberOfObjectsWithKey.Add(curObject->GetAssembly().DatabaseKey, newCompListObj->SelectionItemCount);
			}
		}
	}
}

void USelectionTrayBlockPresetList::ClearPresetList()
{
	ComponentItemMap.Empty();
	AssembliesList->ClearListItems();
}
