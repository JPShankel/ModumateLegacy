// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockDropdownPreset.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateComboBoxString.h"
#include "Components/Image.h"
#include "UI/BIM/BIMBlockNode.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/EditModelUserWidget.h"
#include "UI/BIM/BIMEditColorPicker.h"
#include "UI/LeftMenu/SwapMenuWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"

UBIMBlockDropdownPreset::UBIMBlockDropdownPreset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockDropdownPreset::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();
	
	if (!(ButtonSwap && DropdownList && DropdownList->ComboBoxStringJustification)) 
	{
		return false;
	}

	ButtonSwap->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockDropdownPreset::OnButtonSwapReleased);

	DropdownList->ComboBoxStringJustification->OnSelectionChanged.AddDynamic(this, &UBIMBlockDropdownPreset::OnDropdownListSelectionChanged);

	return true;
}

void UBIMBlockDropdownPreset::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockDropdownPreset::OnButtonSwapReleased()
{
	// Open color picker
	if (FormElement.FormElementWidgetType == EBIMFormElementWidget::ColorPicker)
	{
		FVector2D colorDropdownOffset = DropdownOffset + FVector2D(0.f, OwnerNode->FormItemSize);
		Controller->EditModelUserWidget->BIMDesigner->BIM_EditColorBP->BuildColorPicker(
			ParentBIMDesigner, OwnerNode->ID, FormElement, colorDropdownOffset);
		return;
	}

	// If none of the above, open swap preset menu
	FGuid ownerNodePresetID;
	// Dropdown changes a property of its owner node, it should always have an ID from its owner	
	if (ensureAlways(OwnerNode->ID != BIM_ID_NONE))
	{
		ownerNodePresetID = ParentBIMDesigner->GetPresetID(OwnerNode->ID);
	}

	FBIMTagPath ncpForSwap;
	Controller->GetDocument()->GetPresetCollection().GetNCPForPreset(PresetGUID, ncpForSwap);
	if (ensureAlways(ncpForSwap.Tags.Num() > 0))
	{
		Controller->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->ResetSelectedAndSearchTag();
		Controller->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->SetNCPTagPathAsSelected(ncpForSwap);

		Controller->EditModelUserWidget->SwapMenuWidget->SetSwapMenuAsFromNode(ownerNodePresetID, PresetGUID, OwnerNode->ID, FormElement);
		Controller->EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::SwapMenu);

		Controller->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->ScrollPresetToView(PresetGUID);
	}
}

void UBIMBlockDropdownPreset::BuildDropdownFromPropertyPreset(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FBIMPresetFormElement& InFormElement,  const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNode = InOwnerNode;
	FormElement = InFormElement;
	FGuid::Parse(InFormElement.StringRepresentation, PresetGUID);
	DropdownOffset = InDropdownOffset;

	PresetText->SetVisibility(ESlateVisibility::Visible);
	IconImage->SetVisibility(ESlateVisibility::Visible);
	DropdownList->SetVisibility(ESlateVisibility::Collapsed);
	ButtonSwap->SetVisibility(ESlateVisibility::Visible);

	TextTitle->ChangeText(InFormElement.DisplayName);
	const FBIMPresetInstance* preset = OuterBIMDesigner->InstancePool.PresetCollectionProxy.PresetFromGUID(PresetGUID);
	if (preset != nullptr)
	{
		PresetText->ChangeText(preset->DisplayName);
	}

	// Icon
	bool bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(OuterBIMDesigner->InstancePool.PresetCollectionProxy,false, PresetGUID, IconMaterial, BIM_ID_NONE);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(bCaptureSuccess ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UBIMBlockDropdownPreset::BuildDropdownFromColor(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FBIMPresetFormElement& InFormElement, const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNode = InOwnerNode;
	FormElement = InFormElement;
	DropdownOffset = InDropdownOffset;

	// Set text and label
	TextTitle->ChangeText(FormElement.DisplayName);
	PresetText->ChangeText(FText::FromString(FormElement.StringRepresentation.Left(6)));

	PresetText->SetVisibility(ESlateVisibility::Visible);
	IconImage->SetVisibility(ESlateVisibility::Visible);
	DropdownList->SetVisibility(ESlateVisibility::Collapsed);
	ButtonSwap->SetVisibility(ESlateVisibility::Visible);

	// Icon
	bool bCaptureSuccess = Controller->DynamicIconGenerator->SetIconFromColor(FormElement.StringRepresentation.Left(6), IconMaterial);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(bCaptureSuccess ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UBIMBlockDropdownPreset::BuildDropdownFromStringList(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FBIMPresetFormElement& InFormElement, const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNode = InOwnerNode;
	FormElement = InFormElement;
	DropdownOffset = InDropdownOffset;

	// Set text and label
	TextTitle->ChangeText(FormElement.DisplayName);

	PresetText->SetVisibility(ESlateVisibility::Collapsed);
	IconImage->SetVisibility(ESlateVisibility::Collapsed);
	DropdownList->SetVisibility(ESlateVisibility::Visible);
	ButtonSwap->SetVisibility(ESlateVisibility::Collapsed);

	DropdownList->ComboBoxStringJustification->ClearOptions();
	for (auto& option : InFormElement.SelectionOptions)
	{
		DropdownList->ComboBoxStringJustification->AddOption(option);
	}

	DropdownList->ComboBoxStringJustification->SetSelectedOption(InFormElement.StringRepresentation);
}

void UBIMBlockDropdownPreset::OnDropdownListSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!SelectedItem.IsEmpty() && !FormElement.StringRepresentation.Equals(SelectedItem))
	{
		FormElement.StringRepresentation = SelectedItem;
		ParentBIMDesigner->ApplyBIMFormElement(OwnerNode->ID, FormElement);
	}
}
