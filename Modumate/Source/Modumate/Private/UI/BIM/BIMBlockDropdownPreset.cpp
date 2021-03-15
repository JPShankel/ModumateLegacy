// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockDropdownPreset.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/Image.h"
#include "UI/BIM/BIMBlockNode.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/EditModelUserWidget.h"
#include "UI/BIM/BIMEditColorPicker.h"

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
	
	if (!ButtonSwap)
	{
		return false;
	}
	ButtonSwap->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockDropdownPreset::OnButtonSwapReleased);

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
	// Move swap menu to be in front of this node
	Controller->EditModelUserWidget->ToggleBIMPresetSwapTray(true);

	// Reset the search box in preset list
	Controller->EditModelUserWidget->BIMPresetSwap->ResetSearchBox();

	// Generate list of presets
	Controller->EditModelUserWidget->BIMPresetSwap->CreatePresetListInNodeForSwap(ownerNodePresetID, PresetGUID, OwnerNode->ID, FormElement);
}

void UBIMBlockDropdownPreset::BuildDropdownFromPropertyPreset(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FBIMPresetFormElement& InFormElement,  const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNode = InOwnerNode;
	FormElement = InFormElement;
	FGuid::Parse(InFormElement.StringRepresentation, PresetGUID);
	DropdownOffset = InDropdownOffset;


	TextTitle->ChangeText(InFormElement.DisplayName);
	const FBIMPresetInstance* preset = Controller->GetDocument()->GetPresetCollection().PresetFromGUID(PresetGUID);
	if (preset != nullptr)
	{
		PresetText->ChangeText(preset->DisplayName);
	}

	// Icon
	bool bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(false, PresetGUID, IconMaterial, BIM_ID_NONE);
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

	// Icon
	bool bCaptureSuccess = Controller->DynamicIconGenerator->SetIconFromColor(FormElement.StringRepresentation.Left(6), IconMaterial);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(bCaptureSuccess ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
