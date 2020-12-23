// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockDropdownPreset.h"
#include "UI/BIM/BIMDesigner.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
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

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();
	
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
	if (SwapScope == EBIMValueScope::Color)
	{
		FVector2D colorDropdownOffset = DropdownOffset + FVector2D(0.f, OwnerNode->FormItemSize);
		Controller->EditModelUserWidget->BIMDesigner->BIM_EditColorBP->BuildColorPicker(
			ParentBIMDesigner, OwnerNode->ID, SwapScope, SwapNameType, PresetID, colorDropdownOffset);
		return;
	}

	// If none of the above, open swap preset menu
	FBIMKey ownerNodePresetID;
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
	Controller->EditModelUserWidget->BIMPresetSwap->CreatePresetListInNodeForSwap(ownerNodePresetID, PresetID, OwnerNode->ID, SwapScope, SwapNameType);
}

void UBIMBlockDropdownPreset::BuildDropdownFromPropertyPreset(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const EBIMValueScope& InScope, const FBIMNameType& InNameType, FBIMKey InPresetID, FVector2D InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNode = InOwnerNode;
	SwapScope = InScope;
	SwapNameType = InNameType;
	PresetID = InPresetID;
	DropdownOffset = InDropdownOffset;

	const FBIMPresetInstance* preset = Controller->GetDocument()->PresetManager.CraftingNodePresets.Presets.Find(PresetID);

	// PresetText
	if (preset != nullptr)
	{
		TextTitle->ChangeText(preset->CategoryTitle);
		PresetText->ChangeText(preset->DisplayName);
	}
	else // If no preset is found, use its preset ID
	{
		TextTitle->ChangeText(FText::FromName(BIMNameFromValueScope(SwapScope)));
		PresetText->ChangeText(FText::FromString(PresetID.ToString()));
	}

	// Icon
	bool bCaptureSuccess = false;
	if (SwapScope == EBIMValueScope::Color) // Color is entered as hex, not preset key
	{
		bCaptureSuccess = Controller->DynamicIconGenerator->SetIconFromColor(PresetID, IconMaterial);
	}
	else
	{
		bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(false, PresetID, IconMaterial, IconTexture, BIM_ID_NONE);
	}

	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(bCaptureSuccess ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
