// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockNode.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/BIM/BIMDesigner.h"
#include "Components/Border.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Components/Image.h"
#include "UI/ComponentPresetListItem.h"
#include "UI/ToolTray/ToolTrayBlockAssembliesList.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "Components/VerticalBox.h"
#include "UI/BIM/BIMBlockUserEnterable.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UI/BIM/BIMBlockNodeDirtyTab.h"
#include "UI/BIM/BIMBlockSlotList.h"
#include "UI/BIM/BIMBlockDropdownPreset.h"

UBIMBlockNode::UBIMBlockNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMBlockNode::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

	if (!(ButtonSwapCollapsed && ButtonSwapExpanded && ButtonDeleteCollapsed && ButtonDeleteExpanded && BIMBlockNodeDirty))
	{
		return false;
	}

	ButtonSwapCollapsed->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonSwapReleased);
	ButtonSwapExpanded->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonSwapReleased);
	ButtonDeleteCollapsed->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonDeleteReleased);
	ButtonDeleteExpanded->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonDeleteReleased);
	BIMBlockNodeDirty->ButtonSave->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonDirtySave);
	BIMBlockNodeDirty->ButtonAddNew->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonDirtyAddNew);
	BIMBlockNodeDirty->ButtonCancel->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonDirtyCancel);

	return true;
}

void UBIMBlockNode::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockNode::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DragTick)
	{
		PerformDrag();
	}
}

FReply UBIMBlockNode::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == DragKey)
	{
		if (NodeCollapse && ComponentPresetListItem->GrabHandleImage->GetTickSpaceGeometry().IsUnderLocation(UWidgetLayoutLibrary::GetMousePositionOnPlatform()))
		{
			BeginDrag();
		}
		else if (GrabHandleImage->GetTickSpaceGeometry().IsUnderLocation(UWidgetLayoutLibrary::GetMousePositionOnPlatform()))
		{
			BeginDrag();
		}
	}
	return reply;
}

FReply UBIMBlockNode::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == ToggleCollapseKey && !DragTick)
	{
		UpdateNodeCollapse(!NodeCollapse, true);
	}
	return reply;
}

void UBIMBlockNode::PerformDrag()
{
	float mouseX = 0.f;
	float mouseY = 0.f;
	bool hasMousePosition = Controller->GetMousePosition(mouseX, mouseY);
	FVector2D currentMousePosition = FVector2D(mouseX, mouseY) / UWidgetLayoutLibrary::GetViewportScale(this);

	if (hasMousePosition && !DragReset)
	{
		UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this);
		if (canvasSlot && ParentBIMDesigner)
		{
			// Offset node by mouse delta
			FVector2D newCanvasPosition = ((currentMousePosition - LastMousePosition) / ParentBIMDesigner->GetCurrentZoomScale()) + canvasSlot->GetPosition();
			canvasSlot->SetPosition(newCanvasPosition);
		}
	}
	DragTick = Controller->IsInputKeyDown(DragKey) && hasMousePosition;
	if (DragTick)
	{
		LastMousePosition = currentMousePosition;
		DragReset = false;
	}
	else
	{
		DragReset = true;
		ParentBIMDesigner->GetNodeForReorder(PreDragCanvasPosition, ID);
	}
}

void UBIMBlockNode::OnButtonSwapReleased()
{
	UpdateNodeSwitchState(ENodeWidgetSwitchState::PendingSwap);
	FBIMKey parentPresetID;
	if (ParentID != -1)
	{
		parentPresetID = ParentBIMDesigner->GetPresetID(ParentID);
	}
	// Move swap menu to be in front of this node
	ParentBIMDesigner->UpdateNodeSwapMenuVisibility(ID, true);

	// Generate list of presets
	ParentBIMDesigner->SelectionTray_Block_Swap->CreatePresetListInNodeForSwap(parentPresetID, PresetID, ID);
}

void UBIMBlockNode::OnButtonDeleteReleased()
{
	ParentBIMDesigner->DeleteNode(ID);
}

void UBIMBlockNode::OnButtonDirtySave()
{
	ParentBIMDesigner->SavePresetFromNode(false, ID);
}

void UBIMBlockNode::OnButtonDirtyAddNew()
{
	ParentBIMDesigner->SavePresetFromNode(true, ID);
}

void UBIMBlockNode::OnButtonDirtyCancel()
{
	ECraftingResult result = ParentBIMDesigner->InstancePool.SetNewPresetForNode(Controller->GetDocument()->PresetManager.CraftingNodePresets, ID, PresetID);
	if (result == ECraftingResult::Success)
	{
		ParentBIMDesigner->UpdateCraftingAssembly();
		ParentBIMDesigner->UpdateBIMDesigner();
	}
}

void UBIMBlockNode::UpdateNodeDirty(bool NewDirty)
{
	NodeDirty = NewDirty;
	ESlateVisibility newVisibility = NodeDirty ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	BIMBlockNodeDirty->SetVisibility(newVisibility);
	DirtyStateBorder->SetVisibility(newVisibility);
}

void UBIMBlockNode::UpdateNodeCollapse(bool NewCollapse, bool AllowAutoArrange)
{
	NodeCollapse = NewCollapse;
	ENodeWidgetSwitchState newNodeSwitchState = NodeCollapse ? ENodeWidgetSwitchState::Collapsed : ENodeWidgetSwitchState::Expanded;
	UpdateNodeSwitchState(newNodeSwitchState);
	if (AllowAutoArrange)
	{
		ParentBIMDesigner->AutoArrangeNodes();
	}
}

bool UBIMBlockNode::BuildNode(class UBIMDesigner *OuterBIMDesigner, const FBIMCraftingTreeNodeSharedPtr &Node, bool bAsSlot)
{
	ParentBIMDesigner = OuterBIMDesigner;
	PresetID = Node->PresetID;
	bNodeHasSlotPart = bAsSlot;
	TitleNodeCollapsed->ChangeText(Node->CategoryTitle);
	TitleNodeExpanded->ChangeText(Node->CategoryTitle);
	const FBIMPreset* preset = Controller->GetDocument()->PresetManager.CraftingNodePresets.Presets.Find(PresetID);
	if (preset != nullptr)
	{
		ComponentPresetListItem->MainText->ChangeText(preset->DisplayName);
		Preset_Name->ChangeText(preset->DisplayName);
	}

	if (Button_Debug)
	{
		FString debugString = FString::Printf(TEXT("ID: ")) + FString::FromInt(Node->GetInstanceID()) + LINE_TERMINATOR + PresetID.ToString() + LINE_TERMINATOR + LINE_TERMINATOR + FString::Printf(TEXT("Properties:")) + LINE_TERMINATOR;
		Modumate::FModumateFunctionParameterSet params;
		Node->InstanceProperties.ForEachProperty([this, &params, &debugString](const FString &name, const Modumate::FModumateCommandParameter &param)
		{
			debugString = debugString + name + param.AsString() + LINE_TERMINATOR;
		});
		Button_Debug->SetToolTipText(FText::FromString(debugString));
	}

	ID = Node->GetInstanceID();
	if (Node->ParentInstance != nullptr)
	{
		ParentID = Node->ParentInstance.Pin()->GetInstanceID();
	}

	if (IsKingNode)
	{
		UpdateNodeCollapse(false);
	}
	else
	{
		// TODO: Collapse state of non king node depends on current user selection
		UpdateNodeCollapse(true);
	}

	// Node status
	UpdateNodeDirty(Node->GetPresetStatus(Controller->GetDocument()->PresetManager.CraftingNodePresets) == EBIMPresetEditorNodeStatus::Dirty);

	// Build instance properties
	VerticalBoxProperties->ClearChildren();
	TMap<FString, FBIMNameType> properties;
	Controller->GetDocument()->PresetManager.CraftingNodePresets.GetPropertyFormForPreset(PresetID, properties);
	for (auto& curProperty : properties)
	{
		UBIMBlockUserEnterable *newEnterable = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockUserEnterable>(BIMBlockUserEnterableClass);
		if (newEnterable)
		{
			// Text title
			newEnterable->Text_Title->ChangeText(FText::FromString(curProperty.Key));

			// Text value: user enter-able
			FBIMPropertyValue value(curProperty.Value);
			FString valueString;
			if (Node->InstanceProperties.TryGetProperty(value.Scope, value.Name, valueString))
			{
				newEnterable->BuildEnterableFieldFromProperty(ParentBIMDesigner, ID, value.Scope, value.Name, valueString);
			}

			VerticalBoxProperties->AddChildToVerticalBox(newEnterable);
		}
	}

	TArray<int32> embeddedNodeIds;
	Node->NodesEmbeddedInMe(embeddedNodeIds);
	for (const auto& curEmbeddedID : embeddedNodeIds)
	{
		UBIMBlockDropdownPreset* newDropdown = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockDropdownPreset>(BIMBlockDropdownPresetClass);
		if (newDropdown)
		{
			FVector2D dropDownOffset = FVector2D::ZeroVector;
			dropDownOffset.Y += ExpandedImageSize;
			dropDownOffset.Y += (FormItemSize * VerticalBoxProperties->GetAllChildren().Num());
			
			newDropdown->BuildDropdownFromProperty(ParentBIMDesigner, this, curEmbeddedID, ID, dropDownOffset);
			VerticalBoxProperties->AddChildToVerticalBox(newDropdown);
		}
	}

	// Additional panel for this node if it is part of rigged assembly
	if (bNodeHasSlotPart)
	{
		if (BIMBlockSlotList != nullptr)
		{
			BIMBlockSlotList->BuildSlotAssignmentList(Node);
		}
	}

	// Remove delete button if removal of this node is not allow
	if (bNodeHasSlotPart || !Node->ParentInstance.IsValid() || !Node->ParentInstance.Pin()->CanRemoveChild(Node))
	{
		ButtonDeleteCollapsed->SetVisibility(ESlateVisibility::Collapsed);
		ButtonDeleteExpanded->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Remove grab handle if reordering is not allow
	if (bNodeHasSlotPart || !Node->ParentInstance.IsValid() || !Node->ParentInstance.Pin()->CanReorderChild(Node))
	{
		GrabHandleImage->SetVisibility(ESlateVisibility::Collapsed);
		ComponentPresetListItem->GrabHandleImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	bool bCaptureSuccess = false;
	bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(false, PresetID, IconMaterial, IconTexture, ID);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
		ComponentPresetListItem->IconImage->SetBrushFromMaterial(IconMaterial);
	}

	return true;
}

void UBIMBlockNode::UpdateNodeSwitchState(ENodeWidgetSwitchState NewState)
{
	NodeSwitchState = NewState;
	// Switcher switches between which widget to display, depending on child order
	// Collapsed widget is in first index, expanded widget is second, swap is third
	switch (NodeSwitchState)
	{
	case ENodeWidgetSwitchState::Collapsed:
		NodeSwitcher->SetActiveWidgetIndex(0);
		break;
	case ENodeWidgetSwitchState::Expanded:
		NodeSwitcher->SetActiveWidgetIndex(1);
		break;
	case ENodeWidgetSwitchState::PendingSwap:
		NodeSwitcher->SetActiveWidgetIndex(2);
		break;
	default:
		break;
	}
	ParentBIMDesigner->AutoArrangeNodes();
}

void UBIMBlockNode::BeginDrag()
{
	DragTick = true;
	UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this);
	if (canvasSlot)
	{
		PreDragCanvasPosition = canvasSlot->GetPosition();
	}
}

FVector2D UBIMBlockNode::GetEstimatedNodeSize()
{
	float estimatedSize = 0.f;

	if (NodeDirty)
	{
		estimatedSize += DirtyTabSize;
	}

	if (NodeCollapse)
	{
		estimatedSize += CollapsedNodeSize;
	}
	else
	{
		estimatedSize += ExpandedImageSize;
		estimatedSize += (FormItemSize * VerticalBoxProperties->GetAllChildren().Num());
		estimatedSize += BottomPadding;
	}

	float outWidth = bNodeHasSlotPart ? SlotNodeWidth : NodeWidth;

	return FVector2D(outWidth, estimatedSize);
}
