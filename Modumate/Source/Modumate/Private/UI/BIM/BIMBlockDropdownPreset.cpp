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
	ParentNode->UpdateNodeSwitchState(ENodeWidgetSwitchState::PendingSwap);
	FBIMKey parentPresetID;
	// Dropdown preset is embedded into parent node, it should always have a parentPresetID
	if (ensureAlways(ParentNode->ID != INDEX_NONE))
	{
		parentPresetID = ParentBIMDesigner->GetPresetID(ParentNode->ID);
	}
	// Move swap menu to be in front of this node
	ParentBIMDesigner->UpdateNodeSwapMenuVisibility(NodeID, true, DropdownOffset);

	// Generate list of presets
	ParentBIMDesigner->SelectionTray_Block_Swap->CreatePresetListInNodeForSwap(parentPresetID, PresetID, NodeID);
}

void UBIMBlockDropdownPreset::BuildDropdownFromProperty(class UBIMDesigner* OuterBIMDesigner, UBIMBlockNode* InParentNode, int32 InNodeID, int32 InEmbeddedParentID, FVector2D InDropdownOffset)
{
	ParentBIMDesigner = OuterBIMDesigner;
	ParentNode = InParentNode;
	NodeID = InNodeID;
	EmbeddedParentID = InEmbeddedParentID;
	DropdownOffset = InDropdownOffset;

	const FBIMCraftingTreeNodeSharedPtr nodePtr = ParentBIMDesigner->InstancePool.InstanceFromID(NodeID);
	if (!nodePtr.IsValid())
	{
		return;
	}

	// TextTitle
	TextTitle->ChangeText(FText::FromString(nodePtr->CategoryTitle));

	// Icon
	bool bCaptureSuccess = false;
	bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(false, nodePtr->PresetID, IconMaterial, IconTexture, NodeID);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
	}

	// PresetText
	PresetID = nodePtr->PresetID;
	PresetText->ChangeText(FText::FromString(PresetID.ToString()));
}
