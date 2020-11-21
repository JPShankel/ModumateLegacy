// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockSlotList.h"
#include "UI/BIM/BIMBlockNode.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "UI/BIM/BIMBlockSlotListItem.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"

UBIMBlockSlotList::UBIMBlockSlotList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockSlotList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UBIMBlockSlotList::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockSlotList::BuildSlotAssignmentList(const FBIMPresetEditorNodeSharedPtr& NodePtr)
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	NodeIDToSlotMapItem.Empty();
	VerticalBoxSlots->ClearChildren();

	TArray<FBIMPresetPartSlot> partSlots;
	NodePtr->GetPartSlots(partSlots);
	for (int32 i = 0; i < partSlots.Num(); ++i)
	{
		UBIMBlockSlotListItem* newSlot = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockSlotListItem>(SlotListItemClass);
		if (newSlot)
		{
			const FBIMPresetInstance* preset = controller->GetDocument()->PresetManager.CraftingNodePresets.Presets.Find(partSlots[i].SlotPreset);
			if (preset != nullptr)
			{
				newSlot->TextBlockWidget->ChangeText(preset->DisplayName);
			}		

			newSlot->SlotIndex = i;
			newSlot->ParentID = NodePtr->GetInstanceID();

			if (partSlots[i].PartPreset.IsNone())
			{
				newSlot->ConnectSlotItemToNode(MOD_ID_NONE);
			}
			else
			{
				int32 connectedChildID = MOD_ID_NONE;
				NodePtr->FindNodeIDConnectedToSlot(partSlots[i].SlotPreset, connectedChildID);
				newSlot->ConnectSlotItemToNode(connectedChildID);
				NodeIDToSlotMapItem.Add(connectedChildID, newSlot);
			}

			VerticalBoxSlots->AddChildToVerticalBox(newSlot);
		}
	}
}

void UBIMBlockSlotList::ReleaseSlotAssignmentList()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	for (auto curWidget : VerticalBoxSlots->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			controller->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
}
