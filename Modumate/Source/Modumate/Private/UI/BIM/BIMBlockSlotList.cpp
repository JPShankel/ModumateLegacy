// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockSlotList.h"
#include "UI/BIM/BIMBlockNode.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/EditModelPlayerHUD.h"
#include "Components/VerticalBox.h"
#include "UI/BIM/BIMBlockSlotListItem.h"

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

void UBIMBlockSlotList::BuildSlotAssignmentList(const FBIMCraftingTreeNodeSharedPtr& NodePtr)
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

	for (int32 i = 0; i < NodePtr->AttachedChildren.Num(); ++i)
	{
		UBIMBlockSlotListItem* newSlot = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockSlotListItem>(SlotListItemClass);
		if (newSlot && NodePtr->AttachedChildren[i].IsPart())
		{	
			for (const auto& curChild : NodePtr->AttachedChildren[i].Children)
			{
				NodeIDToSlotMapItem.Add(curChild.Pin()->GetInstanceID(), newSlot);
			}
			FString presetString = NodePtr->AttachedChildren[i].PartSlot.PartPreset.ToString();
			newSlot->TextBlockWidget->ChangeText(FText::FromString(presetString));
			newSlot->SlotID = i;
			VerticalBoxSlots->AddChildToVerticalBox(newSlot);
		}
	}
}
