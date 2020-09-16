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
	TMap<FBIMKey, int32> numberOfObjectsWithKey;
	for (auto& curObject : Controller->EMPlayerState->SelectedObjects)
	{
		if (curObject->GetActor())
		{
			UComponentListObject *compItem = ComponentItemMap.FindRef(curObject->GetAssembly().UniqueKey());
			if (compItem)
			{
				int32 numberOfObj = numberOfObjectsWithKey.FindRef(curObject->GetAssembly().UniqueKey());
				numberOfObjectsWithKey.Add(curObject->GetAssembly().UniqueKey(), numberOfObj + 1);
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
				newCompListObj->UniqueKey = curObject->GetAssembly().UniqueKey();
				newCompListObj->SelectionItemCount = 1;
				AssembliesList->AddItem(newCompListObj);
				ComponentItemMap.Add(curObject->GetAssembly().UniqueKey(), newCompListObj);
				numberOfObjectsWithKey.Add(curObject->GetAssembly().UniqueKey(), newCompListObj->SelectionItemCount);
			}
		}
	}
}

void USelectionTrayBlockPresetList::ClearPresetList()
{
	ComponentItemMap.Empty();
	AssembliesList->ClearListItems();
}
