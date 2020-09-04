// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ComponentAssemblyListItem.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "UI/ComponentListObject.h"
#include "Components/ListView.h"
#include "UI/BIM/BIMDesigner.h"
#include "Components/Sizebox.h"
#include "UI/SelectionTray/SelectionTrayWidget.h"

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
	if (ButtonCancel)
	{
		ButtonCancel->ModumateButton->OnReleased.AddDynamic(this, &UToolTrayBlockAssembliesList::OnButtonCancelReleased);
	}

	return true;
}

void UToolTrayBlockAssembliesList::NativeConstruct()
{
	Super::NativeConstruct();

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	GameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
}

void UToolTrayBlockAssembliesList::CreateAssembliesListForCurrentToolMode()
{
	if (Controller && GameState)
	{
		FPresetManager &presetManager = GameState->Document.PresetManager;
		AssembliesList->ClearListItems();

		EObjectType ot = UModumateTypeStatics::ObjectTypeFromToolMode(Controller->GetToolMode());
		FPresetManager::FAssemblyDataCollection *assemblies = presetManager.AssembliesByObjectType.Find(ot);

		if (assemblies == nullptr)
		{
			return;
		}

		for (auto &kvp : assemblies->DataMap)
		{
			UComponentListObject *newCompListObj = NewObject<UComponentListObject>(this);
			newCompListObj->ItemType = EComponentListItemType::AssemblyListItem;
			newCompListObj->Mode = Controller->GetToolMode();
			newCompListObj->UniqueKey = kvp.Value.UniqueKey();
			AssembliesList->AddItem(newCompListObj);
		}
	}
}

void UToolTrayBlockAssembliesList::CreatePresetListInNodeForSwap(const FName &ParentPresetID, const FName &PresetIDToSwap, int32 NodeID)
{
	SwapType = ESwapType::SwapFromNode;
	CurrentNodeForSwap = NodeID;
	if (Controller && GameState)
	{
		FPresetManager &presetManager = GameState->Document.PresetManager;
		AssembliesList->ClearListItems();

		TArray<FName> availablePresets;
		presetManager.GetAvailablePresetsForSwap(ParentPresetID, PresetIDToSwap, availablePresets);

		for (auto &curPreset : availablePresets)
		{
			UComponentListObject *newCompListObj = NewObject<UComponentListObject>(this);
			newCompListObj->ItemType = EComponentListItemType::SwapDesignerPreset;
			newCompListObj->Mode = Controller->GetToolMode();
			newCompListObj->UniqueKey = curPreset;
			newCompListObj->BIMNodeInstanceID = NodeID;
			AssembliesList->AddItem(newCompListObj);
		}
	}
}

void UToolTrayBlockAssembliesList::CreatePresetListInAssembliesListForSwap(EToolMode ToolMode, const FName &PresetID)
{
	SwapType = ESwapType::SwapFromAssemblyList;
	if (Controller && GameState)
	{
		FPresetManager &presetManager = GameState->Document.PresetManager;
		AssembliesList->ClearListItems();

		TArray<FName> availablePresets;
		presetManager.GetAvailablePresetsForSwap(NAME_None, PresetID, availablePresets);

		for (auto &curPreset : availablePresets)
		{
			UComponentListObject *newCompListObj = NewObject<UComponentListObject>(this);
			newCompListObj->ItemType = EComponentListItemType::SwapListItem;
			newCompListObj->Mode = ToolMode;
			newCompListObj->UniqueKey = curPreset;
			AssembliesList->AddItem(newCompListObj);
		}
	}
}

void UToolTrayBlockAssembliesList::OnButtonAddReleased()
{
	if (Controller && Controller->EditModelUserWidget)
	{
		Controller->EditModelUserWidget->EventNewCraftingAssembly(Controller->GetToolMode());
	}
}

void UToolTrayBlockAssembliesList::OnButtonCancelReleased()
{
	if (Controller && Controller->EditModelUserWidget)
	{
		if (SwapType == ESwapType::SwapFromNode)
		{
			Controller->EditModelUserWidget->BIMDesigner->UpdateNodeSwapMenuVisibility(CurrentNodeForSwap, false);
		}
		else
		{
			Controller->EditModelUserWidget->SelectionTrayWidget->OpenToolTrayForSelection();
		}
	}
}
