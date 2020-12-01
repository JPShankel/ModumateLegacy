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
	OwnerNode->UpdateNodeSwitchState(ENodeWidgetSwitchState::PendingSwap);
	FBIMKey ownerNodePresetID;
	// Dropdown changes a property of its owner node, it should always have an ID from its owner
	if (ensureAlways(OwnerNode->ID != BIM_ID_NONE))
	{
		ownerNodePresetID = ParentBIMDesigner->GetPresetID(OwnerNode->ID);
	}
	// Move swap menu to be in front of this node
	ParentBIMDesigner->UpdateNodeSwapMenuVisibility(OwnerNode->ID, true, DropdownOffset);

	// Reset the search box in preset list
	ParentBIMDesigner->SelectionTray_Block_Swap->ResetSearchBox();

	// Generate list of presets
	ParentBIMDesigner->SelectionTray_Block_Swap->CreatePresetListInNodeForSwap(ownerNodePresetID, PresetID, OwnerNode->ID, SwapScope, SwapNameType);
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

	// Icon
	bool bCaptureSuccess = false;
	bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(false, PresetID, IconMaterial, IconTexture, BIM_ID_NONE);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}
}
