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

void UBIMBlockSlotList::BuildSlotAssignmentList(const FBIMPresetEditorNodeSharedPtr& NodePtr)
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	NodeIDToSlotMapItem.Empty();
	VerticalBoxSlots->ClearChildren();

	for (int32 i = 0; i < NodePtr->WorkingPresetCopy.PartSlots.Num(); ++i)
	{
		const FBIMPresetPartSlot& partSlot = NodePtr->WorkingPresetCopy.PartSlots[i];
		UBIMBlockSlotListItem* newSlot = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockSlotListItem>(SlotListItemClass);
		if (newSlot)// && NodePtr->AttachedChildren[i].IsPart())
		{
			FString presetString = partSlot.PartPreset.ToString();
			newSlot->TextBlockWidget->ChangeText(FText::FromString(presetString));
			newSlot->SlotID = i;
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
