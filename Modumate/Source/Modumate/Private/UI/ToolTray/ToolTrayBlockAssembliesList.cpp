// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ComponentAssemblyListItem.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Components/VerticalBox.h"


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
				UComponentAssemblyListItem *newWidgetListItem = CreateWidget<UComponentAssemblyListItem>(this, ComponentAssemblyListItemClass);
				newWidgetListItem->BuildFromAssembly(assembly);
				AssembliesList->AddChildToVerticalBox(newWidgetListItem);
			}
		}
	}
}
