// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ComponentAssemblyListItem.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "Database/ModumateObjectEnums.h"
#include "UI/ComponentListObject.h"
#include "Components/ListView.h"

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

	Modumate::FPresetManager &presetManager = gameState->Document.PresetManager;

	if (controller && gameState)
	{
		AssembliesList->ClearListItems();

		Modumate::FPresetManager::FAssemblyDataCollection *assemblies = presetManager.AssemblyDBs_DEPRECATED.Find(controller->GetToolMode());
		if (assemblies == nullptr || assemblies->DataMap.Num() == 0)
		{
			EObjectType ot = UModumateTypeStatics::ObjectTypeFromToolMode(controller->GetToolMode());
			assemblies = presetManager.AssembliesByObjectType.Find(ot);
		}

		if (assemblies == nullptr)
		{
			return;
		}

		for (auto &kvp : assemblies->DataMap)
		{
			UComponentListObject *newCompListObj = NewObject<UComponentListObject>(this);
			newCompListObj->ItemType = EComponentListItemType::AssemblyListItem;
			newCompListObj->Mode = controller->GetToolMode();
			newCompListObj->UniqueKey = kvp.Value.DatabaseKey;
			AssembliesList->AddItem(newCompListObj);
		}
	}
}

void UToolTrayBlockAssembliesList::OnButtonAddReleased()
{
	AEditModelPlayerController_CPP* controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->EventNewCraftingAssembly(controller->GetToolMode());
	}
}
