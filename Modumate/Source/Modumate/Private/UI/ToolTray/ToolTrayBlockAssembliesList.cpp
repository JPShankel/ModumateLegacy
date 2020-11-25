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
#include "UI/ComponentPresetListItem.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UI/Custom/ModumateEditableTextBox.h"
#include "Kismet/KismetStringLibrary.h"

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
	if (!(ButtonAdd && Text_SearchBar))
	{
		return false;
	}

	ButtonAdd->ModumateButton->OnReleased.AddDynamic(this, &UToolTrayBlockAssembliesList::OnButtonAddReleased);
	if (ButtonCancel)
	{
		ButtonCancel->ModumateButton->OnReleased.AddDynamic(this, &UToolTrayBlockAssembliesList::OnButtonCancelReleased);
	}
	if (Text_SearchBar)
	{
		Text_SearchBar->ModumateEditableTextBox->OnTextChanged.AddDynamic(this, &UToolTrayBlockAssembliesList::OnSearchBarChanged);
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
	SwapType = ESwapType::SwapFromAssemblyList;
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
			if (IsPresetAvailableForSearch(kvp.Value.UniqueKey()))
			{
				UComponentListObject* newCompListObj = NewObject<UComponentListObject>(this);
				newCompListObj->ItemType = EComponentListItemType::AssemblyListItem;
				newCompListObj->Mode = Controller->GetToolMode();
				newCompListObj->UniqueKey = kvp.Value.UniqueKey();
				AssembliesList->AddItem(newCompListObj);
			}
		}
	}
}

void UToolTrayBlockAssembliesList::CreatePresetListInNodeForSwap(const FBIMKey& ParentPresetID, const FBIMKey& PresetIDToSwap, int32 NodeID, const EBIMValueScope& InScope, const FBIMNameType& InNameType)
{
	SwapType = ESwapType::SwapFromNode;
	NodeParentPresetID = ParentPresetID;
	NodePresetIDToSwap = PresetIDToSwap;
	CurrentNodeForSwap = NodeID;
	SwapScope = InScope;
	SwapNameType = InNameType;

	if (Controller && GameState)
	{
		FPresetManager &presetManager = GameState->Document.PresetManager;
		AssembliesList->ClearListItems();

		TArray<FBIMKey> availablePresets;
		presetManager.GetAvailablePresetsForSwap(ParentPresetID, PresetIDToSwap, availablePresets);

		if (ComponentPresetItem)
		{
			const FBIMPresetInstance* preset = GameState->Document.PresetManager.CraftingNodePresets.Presets.Find(PresetIDToSwap);
			if (preset != nullptr)
			{
				ComponentPresetItem->MainText->ChangeText(preset->DisplayName);
				ComponentPresetItem->CaptureIconForBIMDesignerSwap(Controller, PresetIDToSwap, NodeID);
			}
		}

		for (auto &curPreset : availablePresets)
		{
			if (IsPresetAvailableForSearch(curPreset))
			{
				UComponentListObject* newCompListObj = NewObject<UComponentListObject>(this);
				newCompListObj->ItemType = EComponentListItemType::SwapDesignerPreset;
				newCompListObj->Mode = Controller->GetToolMode();
				newCompListObj->UniqueKey = curPreset;
				newCompListObj->BIMNodeInstanceID = NodeID;
				newCompListObj->SwapScope = SwapScope;
				newCompListObj->SwapNameType = SwapNameType;
				AssembliesList->AddItem(newCompListObj);
			}
		}
	}
}

void UToolTrayBlockAssembliesList::CreatePresetListInAssembliesListForSwap(EToolMode ToolMode, const FBIMKey& PresetID)
{
	SwapType = ESwapType::SwapFromSelection;
	SwapSelectionToolMode = ToolMode;
	SwapSelectionPresetID = PresetID;

	if (Controller && GameState)
	{
		FPresetManager &presetManager = GameState->Document.PresetManager;
		AssembliesList->ClearListItems();

		TArray<FBIMKey> availablePresets;
		presetManager.GetAvailablePresetsForSwap(FBIMKey(), PresetID, availablePresets);

		if (ComponentPresetItem)
		{
			const FBIMPresetInstance* preset = GameState->Document.PresetManager.CraftingNodePresets.Presets.Find(PresetID);
			if (preset != nullptr)
			{
				ComponentPresetItem->MainText->ChangeText(preset->DisplayName);
				ComponentPresetItem->CaptureIconFromPresetKey(Controller, PresetID);
			}
		}

		for (auto &curPreset : availablePresets)
		{
			if (IsPresetAvailableForSearch(curPreset))
			{
				UComponentListObject* newCompListObj = NewObject<UComponentListObject>(this);
				newCompListObj->ItemType = EComponentListItemType::SwapListItem;
				newCompListObj->Mode = ToolMode;
				newCompListObj->UniqueKey = curPreset;
				AssembliesList->AddItem(newCompListObj);
			}
		}
	}
}

bool UToolTrayBlockAssembliesList::IsPresetAvailableForSearch(const FBIMKey& PresetKey)
{
	FString searchSubString = Text_SearchBar->ModumateEditableTextBox->GetText().ToString();
	if (searchSubString.Len() == 0)
	{
		return true;
	}
	if (GameState)
	{
		const FBIMPresetInstance* preset = GameState->Document.PresetManager.CraftingNodePresets.Presets.Find(PresetKey);
		if (preset)
		{
			return UKismetStringLibrary::Contains(preset->DisplayName.ToString(), searchSubString);
		}
	}
	return false;
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

void UToolTrayBlockAssembliesList::OnSearchBarChanged(const FText& NewText)
{
	switch (SwapType)
	{
	case ESwapType::SwapFromNode:
		CreatePresetListInNodeForSwap(NodeParentPresetID, NodePresetIDToSwap, CurrentNodeForSwap, SwapScope, SwapNameType);
		break;
	case ESwapType::SwapFromAssemblyList:
		CreateAssembliesListForCurrentToolMode();
		break;
	case ESwapType::SwapFromSelection:
		CreatePresetListInAssembliesListForSwap(SwapSelectionToolMode, SwapSelectionPresetID);
		break;
	default:
		break;
	}
}
