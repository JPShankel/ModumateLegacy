// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/SelectionTray/SelectionTrayBlockPresetList.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "UI/ComponentAssemblyListItem.h"
#include "Database/ModumateObjectEnums.h"
#include "Components/VerticalBox.h"


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
	TMap<FName, int32> numberOfObjectsWithKey;
	for (auto& curObject : Controller->EMPlayerState->SelectedObjects)
	{
		if (curObject->GetActor())
		{
			UComponentAssemblyListItem *compItem = ComponentItemMap.FindRef(curObject->GetAssembly().DatabaseKey);
			if (compItem)
			{
				int32 numberOfObj = numberOfObjectsWithKey.FindRef(curObject->GetAssembly().DatabaseKey);
				compItem->UpdateSelectionItemCount(numberOfObj + 1);
				numberOfObjectsWithKey.Add(curObject->GetAssembly().DatabaseKey, numberOfObj + 1);
			}
			else
			{
				UComponentAssemblyListItem *newWidgetListItem = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UComponentAssemblyListItem>(ComponentSelectionListItemClass);
				EToolMode mode = UModumateTypeStatics::ToolModeFromObjectType(curObject->GetObjectType());
				newWidgetListItem->BuildAsSelectionItem(Controller, mode, &curObject->GetAssembly(), 0);
				AssembliesList->AddChildToVerticalBox(newWidgetListItem);
				ComponentItemMap.Add(curObject->GetAssembly().DatabaseKey, newWidgetListItem);
			}

		}
	}
}

void USelectionTrayBlockPresetList::ClearPresetList()
{
	ComponentItemMap.Empty();
	AssembliesList->ClearChildren();
}
