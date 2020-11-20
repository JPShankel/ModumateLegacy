// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockSlotListItem.h"
#include "UI/Custom//ModumateTextBlock.h"
#include "UI/ModumateUIStatics.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/EditModelUserWidget.h"
#include "UI/BIM/BIMDesigner.h"
#include "Components/Image.h"

UBIMBlockSlotListItem::UBIMBlockSlotListItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockSlotListItem::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!ButtonSlot)
	{
		return false;
	}

	ButtonSlot->OnReleased.AddDynamic(this, &UBIMBlockSlotListItem::OnButtonSlotReleased);

	return true;
}

void UBIMBlockSlotListItem::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockSlotListItem::ConnectSlotItemToNode(int32 NodeID)
{
	ConnectedNodeID = NodeID;
	ButtonImage->SetBrushFromTexture(ConnectedNodeID == MOD_ID_NONE ? DisconnectedTexture : ConnectedTexture);
}

void UBIMBlockSlotListItem::OnButtonSlotReleased()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget && controller->EditModelUserWidget->BIMDesigner)
	{
		bool hasConnection = ConnectedNodeID != MOD_ID_NONE;
		controller->EditModelUserWidget->BIMDesigner->ToggleSlotNode(ParentID, SlotIndex, !hasConnection);
	}
}
