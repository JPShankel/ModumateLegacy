// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UI/PresetCard/PresetCardHeader.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "DocumentManagement/ModumateDocument.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/LeftMenu/SwapMenuWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/LeftMenu/DeleteMenuWidget.h"

#define LOCTEXT_NAMESPACE "PresetCardHeader"

UPresetCardHeader::UPresetCardHeader(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPresetCardHeader::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// TODO: Create button widget as needed by EPresetCardType
	if (!(ButtonEdit && ButtonSwap && ButtonConfirm && ButtonDuplicate))
	{
		return false;
	}

	ButtonEdit->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonEditReleased);
	ButtonSwap->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonSwapReleased);
	ButtonConfirm->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonConfirmReleased);
	ButtonDuplicate->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonDuplicateReleased);
	ButtonTrash->ModumateButton->OnReleased.AddDynamic(this, &UPresetCardHeader::OnButtonTrashReleased);
	
	return true;
}

void UPresetCardHeader::NativeConstruct()
{
	Super::NativeConstruct();
	EMPlayerController = GetOwningPlayer<AEditModelPlayerController>();
}

void UPresetCardHeader::BuildAsBrowserHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAllowOptions)
{
	PresetGUID = InGUID;
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset)
	{
		CaptureIcon(PresetGUID, NodeID, preset->NodeScope == EBIMValueScope::Assembly);
		ItemDisplayName = preset->DisplayName;
		MainText->ChangeText(ItemDisplayName);
		UpdateOptionButtonSetByPresetCardType(EPresetCardType::Browser, !bAllowOptions);
	}
}

void UPresetCardHeader::BuildAsSwapHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAllowOptions)
{
	PresetGUID = InGUID;
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset)
	{
		// Swap can happen from BIM Designer or select menu
		// Assume icon is assembly if NodeID == none, which happens anytime outside of BIM Designer
		bool bCaptureIconAsAssembly = NodeID == BIM_ID_NONE;
		CaptureIcon(PresetGUID, NodeID, bCaptureIconAsAssembly);
		ItemDisplayName = preset->DisplayName;
		MainText->ChangeText(ItemDisplayName);
		UpdateOptionButtonSetByPresetCardType(EPresetCardType::Swap, !bAllowOptions);
	}
}

void UPresetCardHeader::BuildAsDeleteHeader(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAllowOptions)
{
	PresetGUID = InGUID;
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset)
	{
		CaptureIcon(PresetGUID, NodeID, preset->NodeScope == EBIMValueScope::Assembly);
		ItemDisplayName = preset->DisplayName;
		MainText->ChangeText(ItemDisplayName);
		UpdateOptionButtonSetByPresetCardType(EPresetCardType::Delete, !bAllowOptions);
	}
}

void UPresetCardHeader::BuildAsSelectTrayPresetCard(const FGuid& InGUID, int32 ItemCount, bool bAllowOptions)
{
	PresetGUID = InGUID;
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset)
	{
		CaptureIcon(PresetGUID, BIM_ID_NONE, true); // Only full assembly object are allowed to be selected in scene
		ItemDisplayName = preset->DisplayName;
		UpdateSelectionHeaderItemCount(ItemCount);
		UpdateOptionButtonSetByPresetCardType(EPresetCardType::SelectTray, !bAllowOptions);
	}
}

void UPresetCardHeader::BuildAsSelectTrayPresetCardObjectType(EObjectType InObjectType, int32 ItemCount, bool bAllowOptions)
{
	PresetGUID = FGuid();
	ItemDisplayName = UModumateTypeStatics::GetTextForObjectType(InObjectType);
	UpdateSelectionHeaderItemCount(ItemCount);

	// PresetCards that represent objectType do not have interactions and icons
	UpdateOptionButtonSetByPresetCardType(EPresetCardType::None, !bAllowOptions);
	OverlayIconSmall->SetVisibility(ESlateVisibility::Collapsed);
}

void UPresetCardHeader::BuildAsAssembliesListHeader(const FGuid& InGUID, bool bAllowOptions)
{
	PresetGUID = InGUID;
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset)
	{
		CaptureIcon(PresetGUID, BIM_ID_NONE, true);
		ItemDisplayName = preset->DisplayName;
		MainText->ChangeText(ItemDisplayName);
		UpdateOptionButtonSetByPresetCardType(EPresetCardType::AssembliesList, !bAllowOptions);
	}
}

void UPresetCardHeader::UpdateOptionButtonSetByPresetCardType(EPresetCardType InPresetCardType, bool bHideAllOnly)
{
	// TODO: Create button widget as needed by EPresetCardType
	PresetCardType = InPresetCardType;

	ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);
	ButtonTrash->SetVisibility(ESlateVisibility::Collapsed);
	ButtonEdit->SetVisibility(ESlateVisibility::Collapsed);
	ButtonConfirm->SetVisibility(ESlateVisibility::Collapsed);
	ButtonDuplicate->SetVisibility(ESlateVisibility::Collapsed);

	if (bHideAllOnly)
	{
		return;
	}

	switch (InPresetCardType)
	{
	case EPresetCardType::SelectTray:
		ButtonSwap->SetVisibility(ESlateVisibility::Visible);
		UpdateEditButtonIfPresetIsEditable();
		break;
	case EPresetCardType::Swap:
	case EPresetCardType::Delete:
		ButtonConfirm->SetVisibility(ESlateVisibility::Visible);
		break;
	case EPresetCardType::AssembliesList:
		UpdateEditButtonIfPresetIsEditable();
		ButtonDuplicate->SetVisibility(ESlateVisibility::Visible);
		ButtonTrash->SetVisibility(ESlateVisibility::Visible);
		break;
	case EPresetCardType::Browser:
	case EPresetCardType::None:
	default:
		break;
	}
}

void UPresetCardHeader::UpdateSelectionHeaderItemCount(int32 ItemCount)
{
	if (ItemCount > 1)
	{
		MainText->ChangeText(FText::Format(LOCTEXT("ItemCount", "({0}) {1}"), ItemCount, ItemDisplayName));
	}
	else
	{
		MainText->ChangeText(ItemDisplayName);
	}
}

bool UPresetCardHeader::CaptureIcon(const FGuid& InGUID, const FBIMEditorNodeIDType& NodeID, bool bAsAssembly)
{
	if (!(EMPlayerController && EMPlayerController->DynamicIconGenerator && IconImageSmall))
	{
		return false;
	}

	const auto& preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(InGUID);
	if (!preset)
	{
		OverlayIconSmall->SetVisibility(ESlateVisibility::Collapsed);
		return false;
	}

	bool result = false;
	
	if (bAsAssembly)
	{
		result = EMPlayerController->DynamicIconGenerator->SetIconMeshForAssembly(FBIMPresetCollectionProxy(EMPlayerController->GetDocument()->GetPresetCollection()),InGUID, IconMaterial);
	}
	else
	{
		result = EMPlayerController->DynamicIconGenerator->SetIconMeshForBIMDesigner(FBIMPresetCollectionProxy(EMPlayerController->GetDocument()->GetPresetCollection()),true, InGUID, IconMaterial, NodeID);
	}

	if (result)
	{
		IconImageSmall->SetBrushFromMaterial(IconMaterial);
	}
	OverlayIconSmall->SetVisibility(result ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	return result;
}

void UPresetCardHeader::UpdateEditButtonIfPresetIsEditable()
{
	const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset != nullptr && 
		(preset->NodeScope == EBIMValueScope::Assembly || preset->NodeScope == EBIMValueScope::Part))
	{
		ButtonEdit->SetVisibility(ESlateVisibility::Visible);
		return;
	}
	ButtonEdit->SetVisibility(ESlateVisibility::Collapsed);
}

void UPresetCardHeader::OnButtonEditReleased()
{
	if (EMPlayerController)
	{
		EMPlayerController->EditModelUserWidget->EditExistingAssembly(PresetGUID);
	}
}

void UPresetCardHeader::OnButtonDuplicateReleased()
{
	EMPlayerController->EditModelUserWidget->ToggleSwapMenu(false);

	if (!EMPlayerController)
	{
		return;
	}

	FBIMPresetInstance newPreset;
	if (ensureAlways(EMPlayerController->GetDocument()->DuplicatePreset(GetWorld(), PresetGUID, newPreset)))
	{
		ConfirmGUID(newPreset.GUID);
	}
}

void UPresetCardHeader::OnButtonSwapReleased()
{
	if (!EMPlayerController)
	{
		return;
	}
	FBIMTagPath ncpForSwap;
	EMPlayerController->GetDocument()->GetPresetCollection().GetNCPForPreset(PresetGUID, ncpForSwap);
	if (ensureAlways(ncpForSwap.Tags.Num() > 0))
	{
		EMPlayerController->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->ResetSelectedAndSearchTag();

		EMPlayerController->EditModelUserWidget->SwapMenuWidget->SetSwapMenuAsSelection(PresetGUID);
		EMPlayerController->EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::SwapMenu);
		EMPlayerController->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->SetNCPTagPathAsSelected(ncpForSwap);

		EMPlayerController->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->ScrollPresetToView(PresetGUID);
	}
}

void UPresetCardHeader::OnButtonTrashReleased()
{
	if (ensureAlways(EMPlayerController && 
		EMPlayerController->EditModelUserWidget && 
		EMPlayerController->EditModelUserWidget->DeleteMenuWidget))
	{
		UDeleteMenuWidget* deleteWidget = EMPlayerController->EditModelUserWidget->DeleteMenuWidget;

		// If this preset does not affect any other preset, then just display the modal dialog for delete
		TArray<FGuid> allAncestorPresets;
		if (!EMPlayerController->GetDocument()->PresetIsInUse(PresetGUID))
		{
			deleteWidget->SetPresetToDelete(PresetGUID, FGuid());
			deleteWidget->BuildDeleteModalDialog();
			return;
		}

		// Otherwise, this preset needs to be replaced
		FBIMTagPath ncpForReplace;
		EMPlayerController->GetDocument()->GetPresetCollection().GetNCPForPreset(PresetGUID, ncpForReplace);
		if (ensureAlways(ncpForReplace.Tags.Num() > 0))
		{
			EMPlayerController->EditModelUserWidget->DeleteMenuWidget->NCPNavigatorWidget->ResetSelectedAndSearchTag();

			deleteWidget->SetPresetToDelete(PresetGUID, FGuid());
			EMPlayerController->EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::DeleteMenu);
			EMPlayerController->EditModelUserWidget->DeleteMenuWidget->NCPNavigatorWidget->SetNCPTagPathAsSelected(ncpForReplace);
		}
	}
}

void UPresetCardHeader::ConfirmGUID(const FGuid& InGUID)
{
	const USwapMenuWidget* swapMenu = EMPlayerController->EditModelUserWidget->SwapMenuWidget;

	if (PresetCardType == EPresetCardType::Swap)
	{
		if (swapMenu->CurrentSwapMenuType == ESwapMenuType::SwapFromSelection)
		{
			// Swap from selection
			const FBIMPresetInstance* preset = EMPlayerController->GetDocument()->GetPresetCollection().PresetFromGUID(InGUID);
			if (preset)
			{
				const FBIMAssemblySpec* assembly = EMPlayerController->GetDocument()->GetPresetCollection().GetAssemblyByGUID(
					UModumateTypeStatics::ToolModeFromObjectType(preset->ObjectType), preset->GUID);
				if (assembly)
				{
					TArray<int32> selectedObjIDs;
					for (auto& moi : EMPlayerController->EMPlayerState->SelectedObjects)
					{
						if (moi->GetAssembly().UniqueKey() == swapMenu->GetPresetGUIDToSwap())
						{
							selectedObjIDs.Add(moi->ID);
						}
					}
					EMPlayerController->GetDocument()->SetAssemblyForObjects(GetWorld(), selectedObjIDs, *assembly);
				}
			}
		}
		else if (swapMenu->CurrentSwapMenuType == ESwapMenuType::SwapFromNode)
		{
			// Swap from node
			EMPlayerController->EditModelUserWidget->ToggleSwapMenu(false);
			// This swap can be either from node, or from one the properties inside this node
			// If the form item is uninitialized, then we're swapping the whole preset, otherwise it's a property or custom data field
			if (swapMenu->BIMPresetFormElementToSwap.FieldType == EBIMPresetEditorField::None)
			{
				EMPlayerController->EditModelUserWidget->BIMDesigner->SetPresetForNodeInBIMDesigner(
					swapMenu->BIMNodeIDToSwap, InGUID);
			}
			else
			{
				FBIMPresetFormElement swapFormElement = swapMenu->BIMPresetFormElementToSwap;
				swapFormElement.StringRepresentation = InGUID.ToString();
				EMPlayerController->EditModelUserWidget->BIMDesigner->ApplyBIMFormElement(
					swapMenu->BIMNodeIDToSwap, swapFormElement);
			}
		}
	}
	else if (PresetCardType == EPresetCardType::AssembliesList)
	{
		if (EMPlayerController && EMPlayerController->EMPlayerState && EMPlayerController->EditModelUserWidget)
		{
			EMPlayerController->EMPlayerState->SetAssemblyForToolMode(EMPlayerController->GetToolMode(), InGUID);
			EMPlayerController->EditModelUserWidget->RefreshAssemblyList(true);
		}
	}
	else if (PresetCardType == EPresetCardType::Delete)
	{
		if (ensureAlways(EMPlayerController && 
			EMPlayerController->EditModelUserWidget && 
			EMPlayerController->EditModelUserWidget->DeleteMenuWidget))
		{
			UDeleteMenuWidget* deleteWidget = EMPlayerController->EditModelUserWidget->DeleteMenuWidget;
			deleteWidget->SetPresetToDelete(deleteWidget->GetPresetGUIDToDelete(), PresetGUID);
			deleteWidget->BuildDeleteModalDialog();
		}
	}
}

void UPresetCardHeader::OnButtonConfirmReleased()
{
	ConfirmGUID(PresetGUID);
}

#undef LOCTEXT_NAMESPACE