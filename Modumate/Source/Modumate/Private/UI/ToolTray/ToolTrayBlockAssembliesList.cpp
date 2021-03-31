// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/EditModelUserWidget.h"
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
#include "UI/PresetCard/PresetCardItemObject.h"

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
						UPresetCardItemObject* newPresetCardItemObj = NewObject<UPresetCardItemObject>(this);
						newPresetCardItemObj->PresetCardType = EPresetCardType::AssembliesList;
						newPresetCardItemObj->ObjectType = Preset.ObjectType;
						newPresetCardItemObj->PresetGuid = Preset.GUID;
						newPresetCardItemObj->ParentToolTrayBlockAssembliesList = this;
						AssembliesList->AddItem(newPresetCardItemObj);
					}
				}
			}
		);
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

void UToolTrayBlockAssembliesList::RefreshAssembliesListView()
{
	AssembliesList->RequestRefresh();
}

void UToolTrayBlockAssembliesList::OnButtonAddReleased()
{
	if (Controller && Controller->EditModelUserWidget)
	{
		Controller->EditModelUserWidget->EventNewCraftingAssembly(Controller->GetToolMode());
	}
}

void UToolTrayBlockAssembliesList::OnSearchBarChanged(const FText& NewText)
{
	CreateAssembliesListForCurrentToolMode();
}
