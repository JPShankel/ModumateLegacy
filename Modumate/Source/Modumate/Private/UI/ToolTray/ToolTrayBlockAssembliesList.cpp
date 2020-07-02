// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ComponentAssemblyListItem.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Components/VerticalBox.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/ToolTray/ToolTrayWidget.h"
#include "UI/EditModelUserWidget.h"

UToolTrayBlockAssembliesList::UToolTrayBlockAssembliesList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UToolTrayBlockAssembliesList::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (!ButtonAdd)
	{
		return false;
	}

	ButtonAdd->ModumateButton->OnReleased.AddDynamic(this, &UToolTrayBlockAssembliesList::OnButtonAddReleased);

	return true;
}

void UToolTrayBlockAssembliesList::NativeConstruct()
{
	Super::NativeConstruct();
}

void UToolTrayBlockAssembliesList::CreateAssembliesListForCurrentToolMode()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	if (controller && gameState && ComponentAssemblyListItemClass)
	{
		AssembliesList->ClearChildren();
		TArray<FShoppingItem> items = gameState->GetAssembliesForToolMode(controller->GetToolMode());
		for (FShoppingItem& curItem : items)
		{
			const FModumateObjectAssembly *assembly = gameState->Document.PresetManager.GetAssemblyByKey(controller->GetToolMode(), curItem.Key);
			if (assembly)
			{
				UComponentAssemblyListItem *newWidgetListItem = controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UComponentAssemblyListItem>(ComponentAssemblyListItemClass);
				newWidgetListItem->ToolTrayBlockAssembliesList = this;
				newWidgetListItem->BuildFromAssembly(controller, controller->GetToolMode(), assembly);
				AssembliesList->AddChildToVerticalBox(newWidgetListItem);
			}
		}
	}
}

void UToolTrayBlockAssembliesList::OnButtonAddReleased()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && ToolTray && ToolTray->EditModelUserWidget)
	{
		ToolTray->EditModelUserWidget->EventNewCraftingAssembly(controller->GetToolMode());
	}
}
