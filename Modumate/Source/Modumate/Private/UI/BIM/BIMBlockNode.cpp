// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMBlockNode.h"
#include "UnrealClasses/EditModelPlayerController.h"
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
#include "Components/VerticalBox.h"
#include "UI/BIM/BIMBlockUserEnterable.h"
#include "UI/EditModelPlayerHUD.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UI/Custom/ModumateEditableTextBoxUserWidget.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UI/BIM/BIMBlockNodeDirtyTab.h"
#include "UI/BIM/BIMBlockSlotList.h"
#include "UI/BIM/BIMBlockDropdownPreset.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/EditModelUserWidget.h"
#include "UI/Debugger/BIMDebugger.h"
#include "UI/LeftMenu/SwapMenuWidget.h"
#include "UI/LeftMenu/NCPNavigator.h"

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

	Controller = GetOwningPlayer<AEditModelPlayerController>();

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

	if (Button_Connector)
	{
		Button_Connector->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonConnectorReleased);
	}

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
	bMouseButtonDownOnNode = true;
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
	if (InMouseEvent.GetEffectingButton() == ToggleCollapseKey && !DragTick && bMouseButtonDownOnNode == true)
	{
		ParentBIMDesigner->SetNodeAsSelected(ID);
	}
	bMouseButtonDownOnNode = false;
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
	ParentBIMDesigner->SetNodeAsSelected(ID);
	FGuid parentPresetID;
	if (!ParentID.IsNone())
	{
		parentPresetID = ParentBIMDesigner->GetPresetID(ParentID);
	}

	FBIMTagPath ncpForSwap;
	Controller->GetDocument()->GetPresetCollection().GetNCPForPreset(PresetID, ncpForSwap);
	if (ensureAlways(ncpForSwap.Tags.Num() > 0))
	{
		Controller->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->ResetSelectedAndSearchTag();
		Controller->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->SetNCPTagPathAsSelected(ncpForSwap);

		Controller->EditModelUserWidget->SwapMenuWidget->SetSwapMenuAsFromNode(parentPresetID, PresetID, ID, FBIMPresetFormElement());
		Controller->EditModelUserWidget->SwitchLeftMenu(ELeftMenuState::SwapMenu);

		Controller->EditModelUserWidget->SwapMenuWidget->NCPNavigatorWidget->ScrollPresetToView(PresetID);
	}
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
	EBIMResult result = ParentBIMDesigner->InstancePool.SetNewPresetForNode(ID, PresetID);
	if (result == EBIMResult::Success)
	{
		ParentBIMDesigner->UpdateCraftingAssembly();
		// Mark this node as the last selected node
		ParentBIMDesigner->SelectedNodeID = ID;
		ParentBIMDesigner->UpdateBIMDesigner();
	}
}

void UBIMBlockNode::OnButtonConnectorReleased()
{
#if WITH_EDITOR
	Controller->EditModelUserWidget->ShowBIMDebugger(true);
	Controller->EditModelUserWidget->BIMDebugger->DebugBIMPreset(PresetID);
#endif
}

void UBIMBlockNode::UpdateNodeDirty(bool NewDirty)
{
	NodeDirty = NewDirty;
	ESlateVisibility newVisibility = NodeDirty ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	BIMBlockNodeDirty->SetVisibility(newVisibility);
	DirtyStateBorder->SetVisibility(newVisibility);
}

void UBIMBlockNode::UpdateNodeCollapse(bool NewCollapse)
{
	NodeCollapse = NewCollapse;
	ENodeWidgetSwitchState newNodeSwitchState = NodeCollapse ? ENodeWidgetSwitchState::Collapsed : ENodeWidgetSwitchState::Expanded;
	UpdateNodeSwitchState(newNodeSwitchState);
}

void UBIMBlockNode::UpdateNodeHidden(bool NewHide)
{
	bNodeIsHidden = NewHide;
	SetVisibility(bNodeIsHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

bool UBIMBlockNode::BuildNode(class UBIMDesigner *OuterBIMDesigner, const FBIMPresetEditorNodeSharedPtr &Node, bool bAsSlot)
{
	IsRootNode = Node->ParentInstance == nullptr;
	ParentBIMDesigner = OuterBIMDesigner;
	PresetID = Node->WorkingPresetCopy.GUID;
	bNodeHasSlotPart = bAsSlot;
	TitleNodeCollapsed->ChangeText(Node->CategoryTitle);
	TitleNodeExpanded->ChangeText(Node->CategoryTitle);

	FText presetDisplayName;
	if (Node->WorkingPresetCopy.TryGetProperty(BIMPropertyNames::Name, presetDisplayName))
	{
		ComponentPresetListItem->MainText->ChangeText(presetDisplayName);
		Preset_Name->ChangeText(presetDisplayName);
	}

	// Can't swap in root node
	ButtonSwapCollapsed->SetVisibility(IsRootNode ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ButtonSwapExpanded->SetVisibility(IsRootNode ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

#if WITH_EDITOR
	if (Button_Connector)
	{
		FString debugString = 
			FString::Printf(TEXT("ID: ")) + Node->GetInstanceID().ToString() + LINE_TERMINATOR 
			+ Node->WorkingPresetCopy.GUID.ToString() + LINE_TERMINATOR
			+ PresetID.ToString() + LINE_TERMINATOR;
		
		Button_Connector->SetToolTipText(FText::FromString(debugString));
	}
#endif

	ID = Node->GetInstanceID();
	if (Node->ParentInstance != nullptr)
	{
		ParentID = Node->ParentInstance.Pin()->GetInstanceID();
	}

	// Node status
	UpdateNodeDirty(Node->GetPresetStatus() == EBIMPresetEditorNodeStatus::Dirty);

	// Build instance properties
	VerticalBoxProperties->ClearChildren();
	FBIMPresetForm presetForm;
	Node->GetPresetForm(presetForm);
	for (auto& curProperty : presetForm.Elements)
	{
		// TODO: Need more format info to determine if this should be drop-down
		// For now just use dropdown if propertyValue.Name == BIMPropertyNames::AssetID
		switch (curProperty.FormElementWidgetType)
		{
			case EBIMFormElementWidget::ColorPicker:
			case EBIMFormElementWidget::GUIDSwap:
			{
				UBIMBlockDropdownPreset* newDropdown = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockDropdownPreset>(BIMBlockDropdownPresetClass);
				if (newDropdown)
				{
					// The location of the dropdown menu if the swap button is pressed
					FVector2D dropDownOffset = FVector2D::ZeroVector;
					dropDownOffset.Y += ExpandedImageSize;
					dropDownOffset.Y += (FormItemSize * VerticalBoxProperties->GetAllChildren().Num());

					// Store preset into new dropdown
					if (curProperty.FormElementWidgetType == EBIMFormElementWidget::ColorPicker)
					{
						newDropdown->BuildDropdownFromColor(ParentBIMDesigner, this, curProperty, dropDownOffset);
					}
					else
					{
						newDropdown->BuildDropdownFromPropertyPreset(ParentBIMDesigner, this, curProperty, dropDownOffset);
					}
					VerticalBoxProperties->AddChildToVerticalBox(newDropdown);
				}
			}
			break;

			case EBIMFormElementWidget::TextEntry:
			{
				UBIMBlockUserEnterable* newEnterable = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockUserEnterable>(BIMBlockUserEnterableClass);
				if (newEnterable)
				{
					// Text title
					newEnterable->Text_Title->ChangeText(curProperty.DisplayName);
					newEnterable->BuildEnterableFieldFromProperty(ParentBIMDesigner, ID, curProperty);
					VerticalBoxProperties->AddChildToVerticalBox(newEnterable);
				}
			}
			break;

			default: ensureAlways(false);
		};
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
	ESlateVisibility removeHandleVisibility =
		bNodeHasSlotPart || !Node->ParentInstance.IsValid() || !Node->ParentInstance.Pin()->CanRemoveChild(Node) ?
		ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	ButtonDeleteCollapsed->SetVisibility(removeHandleVisibility);
	ButtonDeleteExpanded->SetVisibility(removeHandleVisibility);

	// Remove grab handle visibility if reordering is not allow
	ESlateVisibility reorderHandleVisibility =
		bNodeHasSlotPart || !Node->ParentInstance.IsValid() || !Node->ParentInstance.Pin()->CanReorderChild(Node) ?
		ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	GrabHandleImage->SetVisibility(reorderHandleVisibility);
	ComponentPresetListItem->GrabHandleImage->SetVisibility(reorderHandleVisibility);

	bool bCaptureSuccess = false;
	// TODO Wesley: use cached icon only when node is not dirty. 
	// Find out why non-assembly icons are not using the correct cached texture 
	bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(OuterBIMDesigner->InstancePool.PresetCollectionProxy,false, PresetID, IconMaterial, ID, false);
	if (bCaptureSuccess)
	{
		IconImage->SetBrushFromMaterial(IconMaterial);
		ComponentPresetListItem->IconImage->SetBrushFromMaterial(IconMaterial);
	}
	IconImage->SetVisibility(bCaptureSuccess ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

	return true;
}

void UBIMBlockNode::ReleaseNode()
{
	if (ensure(Controller))
	{
		for (auto curWidget : VerticalBoxProperties->GetAllChildren())
		{
			UUserWidget* asUserWidget = Cast<UUserWidget>(curWidget);
			if (asUserWidget)
			{
				Controller->HUDDrawWidget->UserWidgetPool.Release(asUserWidget);
			}
		}
		if (BIMBlockSlotList)
		{
			BIMBlockSlotList->ReleaseSlotAssignmentList();
		}
		Controller->HUDDrawWidget->UserWidgetPool.Release(this);
	}
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
	default:
		break;
	}
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

void UBIMBlockNode::SetNodeAsHighlighted(bool NewHighlight)
{
	bNodeHighlight = NewHighlight;
	float opacity = bNodeHighlight ? 1.f : NodeNonHighlightOpacity;
	SetRenderOpacity(opacity);
	ComponentPresetListItem->IconImage->SetRenderOpacity(opacity);

	UMaterialInstanceDynamic* dynMat = ComponentPresetListItem->IconImage->GetDynamicMaterial();
	dynMat->SetScalarParameterValue(NodeAlphaParamName, bNodeHighlight ? 1.f : 0.f);
}

void UBIMBlockNode::ToggleConnectorVisibilityToChildren(bool HasChildren)
{
	if (!Button_Connector)
	{
		return;
	}

	bool bIsExpandedSlotNode = HasChildren && bNodeHasSlotPart && !NodeCollapse;
	// Allow connector to always be on when in editor
	bool bAlwaysVisible = false;
#if WITH_EDITOR
	bAlwaysVisible = true;
#endif

	if (bAlwaysVisible || (!bIsExpandedSlotNode && HasChildren))
	{
		Button_Connector->SetVisibility(ESlateVisibility::Visible);
		Button_Connector->SetRenderTranslation(FVector2D(0.f, GetEstimatedNodeSize().Y / 2.f));
	}
	else
	{
		Button_Connector->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FVector2D UBIMBlockNode::GetEstimatedNodeSize()
{
	if (bNodeIsHidden)
	{
		return 0.f;
	}

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

	float outWidth = bNodeHasSlotPart ? (NodeCollapse ? SlotNodeWidthCollapsed : SlotNodeWidthExpanded) : NodeWidth;

	return FVector2D(outWidth, estimatedSize);
}
