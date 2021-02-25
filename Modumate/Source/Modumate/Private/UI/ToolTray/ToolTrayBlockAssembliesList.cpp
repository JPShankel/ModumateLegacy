// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UI/ComponentAssemblyListItem.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
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
#include "UI/BIM/BIMBlockNCPNavigator.h"

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

	Controller = GetOwningPlayer<AEditModelPlayerController>();
	GameState = GetWorld()->GetGameState<AEditModelGameState>();
}

void UToolTrayBlockAssembliesList::CreateAssembliesListForCurrentToolMode()
{
	SwapType = ESwapType::SwapFromAssemblyList;
	if (Controller && GameState)
	{
		EObjectType ot = UModumateTypeStatics::ObjectTypeFromToolMode(Controller->GetToolMode());
		AssembliesList->ClearListItems();

		GameState->Document->GetPresetCollection().ForEachPreset(
			[this,ot](const FBIMPresetInstance& Preset)
			{
				if (Preset.ObjectType == ot)
				{
					if (IsPresetAvailableForSearch(Preset.GUID))
					{
						UComponentListObject* newCompListObj = NewObject<UComponentListObject>(this);
						newCompListObj->ItemType = EComponentListItemType::AssemblyListItem;
						newCompListObj->Mode = Controller->GetToolMode();
						newCompListObj->UniqueKey = Preset.GUID;
						AssembliesList->AddItem(newCompListObj);
					}
				}
			}
		);
	}
}

void UToolTrayBlockAssembliesList::CreatePresetListInNodeForSwap(const FGuid& ParentPresetID, const FGuid& PresetIDToSwap, const FBIMEditorNodeIDType& NodeID, const FBIMPresetFormElement& InFormElement)
{
	SwapType = ESwapType::SwapFromNode;
	CurrentNodeForSwap = NodeID;
	FormElement = InFormElement;

	if (Controller && GameState)
	{
		// Build icon for current preset to swap
		if (ComponentPresetItem)
		{
			const FBIMPresetInstance* preset = GameState->Document->GetPresetCollection().PresetFromGUID(PresetIDToSwap);
			if (preset != nullptr)
			{
				ComponentPresetItem->MainText->ChangeText(preset->DisplayName);
				ComponentPresetItem->CaptureIconForBIMDesignerSwap(Controller, PresetIDToSwap, NodeID);
			}
		}

		if (NCPNavigator)
		{
			FBIMTagPath ncpFromPreset;
			Controller->GetDocument()->GetPresetCollection().GetNCPForPreset(PresetIDToSwap, ncpFromPreset);
			NCPNavigator->SetVisibility(ESlateVisibility::Visible);
			NCPNavigator->BuildNCPNavigator(ncpFromPreset);
		}

		AvailableBIMDesignerPresets.Empty();
		Controller->GetDocument()->GetPresetCollection().GetAvailablePresetsForSwap(ParentPresetID, PresetIDToSwap, AvailableBIMDesignerPresets);
		AddBIMDesignerPresetsToList();
	}
}

void UToolTrayBlockAssembliesList::CreatePresetListForSwapFronNCP(const FBIMTagPath& InNCP)
{
	if (NCPNavigator)
	{
		NCPNavigator->SetVisibility(ESlateVisibility::Visible);
		NCPNavigator->BuildNCPNavigator(InNCP);
	}

	AvailableBIMDesignerPresets.Empty();
	GameState->Document->GetPresetCollection().GetPresetsForNCP(InNCP, AvailableBIMDesignerPresets);
	AddBIMDesignerPresetsToList();
}

void UToolTrayBlockAssembliesList::AddBIMDesignerPresetsToList()
{
	// Available presets for SwapDesignerPreset
	AssembliesList->ClearListItems();
	for (auto& curPreset : AvailableBIMDesignerPresets)
	{
		if (IsPresetAvailableForSearch(curPreset))
		{
			UComponentListObject* newCompListObj = NewObject<UComponentListObject>(this);
			newCompListObj->ItemType = EComponentListItemType::SwapDesignerPreset;
			newCompListObj->Mode = Controller->GetToolMode();
			newCompListObj->UniqueKey = curPreset;
			newCompListObj->FormElement = FormElement;
			newCompListObj->BIMNodeInstanceID = CurrentNodeForSwap;
			AssembliesList->AddItem(newCompListObj);
		}
	}
}

void UToolTrayBlockAssembliesList::CreatePresetListInAssembliesListForSwap(EToolMode ToolMode, const FGuid& PresetID)
{
	SwapType = ESwapType::SwapFromSelection;
	SwapSelectionToolMode = ToolMode;
	SwapSelectionPresetID = PresetID;

	if (Controller && GameState)
	{
		AssembliesList->ClearListItems();

		TArray<FGuid> availablePresets;
		GameState->Document->GetPresetCollection().GetAvailablePresetsForSwap(FGuid(), PresetID, availablePresets);

		if (ComponentPresetItem)
		{
			const FBIMPresetInstance* preset = GameState->Document->GetPresetCollection().PresetFromGUID(PresetID);
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

bool UToolTrayBlockAssembliesList::IsPresetAvailableForSearch(const FGuid& PresetKey)
{
	FString searchSubString = Text_SearchBar->ModumateEditableTextBox->GetText().ToString();
	if (searchSubString.Len() == 0)
	{
		return true;
	}
	if (GameState)
	{
		const FBIMPresetInstance* preset = GameState->Document->GetPresetCollection().PresetFromGUID(PresetKey);
		if (preset)
		{
			return UKismetStringLibrary::Contains(preset->DisplayName.ToString(), searchSubString);
		}
	}
	return false;
}

void UToolTrayBlockAssembliesList::ResetSearchBox()
{
	Text_SearchBar->ModumateEditableTextBox->SetText(FText::GetEmpty());
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
			Controller->EditModelUserWidget->ToggleBIMPresetSwapTray(false);
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
		AddBIMDesignerPresetsToList();
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
