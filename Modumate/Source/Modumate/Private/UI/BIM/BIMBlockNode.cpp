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
#include "Kismet/KismetRenderingLibrary.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UI/BIM/BIMBlockNodeDirtyTab.h"

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
	BIMBlockNodeDirty->ButtonSaveAs->ModumateButton->OnReleased.AddDynamic(this, &UBIMBlockNode::OnButtonDirtySaveAs);

	return true;
}

void UBIMBlockNode::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMBlockNode::NativeDestruct()
{
	Super::NativeDestruct();
	if (IconRenderTarget)
	{
		IconRenderTarget->ReleaseResource();
	}
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

void UBIMBlockNode::OnButtonDirtySaveAs()
{
	ParentBIMDesigner->SavePresetFromNode(false, ID);
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

bool UBIMBlockNode::BuildNode(class UBIMDesigner *OuterBIMDesigner, const FBIMCraftingTreeNodeSharedPtr &Node)
{
	ParentBIMDesigner = OuterBIMDesigner;
	PresetID = Node->PresetID;

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

	// TODO: Use icon caching instead of creating new render target each time
	if (!IconRenderTarget)
	{
		IconRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), 256, 256, ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor::Black, true);
	}
	bool bCaptureSuccess = false;
	if (IsKingNode) // Root node should be able to create full assembly
	{
		EToolMode toolMode = EToolMode::VE_NONE;
		bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForAssemblyByToolMode(true, Node->PresetID, toolMode, IconRenderTarget);
	}
	else
	{
		bCaptureSuccess = Controller->DynamicIconGenerator->SetIconMeshForBIMDesigner(PresetID, IconRenderTarget);
	}
	if (bCaptureSuccess)
	{
		static const FName textureParamName(TEXT("Texture"));
		IconImage->GetDynamicMaterial()->SetTextureParameterValue(textureParamName, IconRenderTarget);
		ComponentPresetListItem->IconImage->GetDynamicMaterial()->SetTextureParameterValue(textureParamName, IconRenderTarget);
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

	return FVector2D(NodeWidth, estimatedSize);
}
