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
			ParentBIMDesigner, OwnerNode->ID, SwapScope, SwapNameType, HexValue, colorDropdownOffset);
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
	Controller->EditModelUserWidget->BIMPresetSwap->CreatePresetListInNodeForSwap(ownerNodePresetID, PresetGUID, OwnerNode->ID, SwapScope, SwapNameType);
}

void UBIMBlockDropdownPreset::BuildDropdownFromPropertyPreset(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const EBIMValueScope& InScope, const FBIMNameType& InNameType, const FGuid& InPresetID, const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNode = InOwnerNode;
	SwapScope = InScope;
	SwapNameType = InNameType;
	PresetGUID = InPresetID;
	DropdownOffset = InDropdownOffset;

	const FBIMPresetInstance* preset = Controller->GetDocument()->PresetManager.CraftingNodePresets.PresetFromGUID(PresetGUID);

	// Set text and label
	if (preset != nullptr)
	{
		TextTitle->ChangeText(preset->CategoryTitle);
		PresetText->ChangeText(preset->DisplayName);
	}

	// Icon
	bool bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(false, PresetGUID, IconMaterial, IconTexture, BIM_ID_NONE);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(bCaptureSuccess ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UBIMBlockDropdownPreset::BuildDropdownFromColor(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InOwnerNode, const FString& InHex, const FVector2D& InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	OwnerNode = InOwnerNode;
	SwapScope = EBIMValueScope::Color;
	SwapNameType = BIMPropertyNames::HexValue;
	HexValue = InHex;
	DropdownOffset = InDropdownOffset;

	// Set text and label
	TextTitle->ChangeText(FText::FromName(BIMNameFromValueScope(SwapScope)));
	PresetText->ChangeText(FText::FromString(HexValue.Left(6)));

	// Icon
	bool bCaptureSuccess = Controller->DynamicIconGenerator->SetIconFromColor(HexValue, IconMaterial);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(bCaptureSuccess ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
