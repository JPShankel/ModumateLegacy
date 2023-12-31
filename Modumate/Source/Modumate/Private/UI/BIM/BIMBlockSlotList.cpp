// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockSlotList.h"
#include "UI/BIM/BIMBlockNode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "UI/BIM/BIMBlockSlotListItem.h"
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
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();

	// Release slot item widgets
	for (auto curWidget : VerticalBoxSlots->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			controller->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
	NodeIDToSlotMapItem.Empty();
	VerticalBoxSlots->ClearChildren();

	TArray<FBIMPresetPartSlot> partSlots;
	NodePtr->GetPartSlots(partSlots);
	for (int32 i = 0; i < partSlots.Num(); ++i)
	{
		UBIMBlockSlotListItem* newSlot = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockSlotListItem>(SlotListItemClass);
		if (newSlot)
		{
			const FBIMPresetInstance* preset = controller->GetDocument()->GetPresetCollection().PresetFromGUID(partSlots[i].SlotPresetGUID);
			if (preset != nullptr)
			{
				newSlot->TextBlockWidget->ChangeText(preset->DisplayName);
			}		

			newSlot->SlotIndex = i;
			newSlot->ParentID = NodePtr->GetInstanceID();

			if (!partSlots[i].PartPresetGUID.IsValid())
			{
				newSlot->ConnectSlotItemToNode(BIM_ID_NONE);
			}
			else
			{
				FBIMEditorNodeIDType connectedChildID;
				NodePtr->FindNodeIDConnectedToSlot(partSlots[i].SlotPresetGUID, connectedChildID);
				newSlot->ConnectSlotItemToNode(connectedChildID);
				NodeIDToSlotMapItem.Add(connectedChildID, newSlot);
			}

			VerticalBoxSlots->AddChildToVerticalBox(newSlot);
		}
	}
}

void UBIMBlockSlotList::ReleaseSlotAssignmentList()
{
	AEditModelPlayerController* controller = GetOwningPlayer<AEditModelPlayerController>();
	for (auto curWidget : VerticalBoxSlots->GetAllChildren())
	{
		UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
		if (asUserWidget)
		{
			controller->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
		}
	}
}
