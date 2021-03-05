// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMDesigner.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/CanvasPanel.h"
#include "UI/BIM/BIMBlockNode.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UI/EditModelPlayerHUD.h"
#include "ModumateCore/ModumateSlateHelper.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "UI/BIM/BIMBlockAddLayer.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "Components/Sizebox.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/ThumbnailCacheManager.h"
#include "UI/BIM/BIMBlockSlotList.h"
#include "UI/BIM/BIMBlockSlotListItem.h"
#include "UnrealClasses/DynamicIconGenerator.h"
#include "UI/BIM/BIMBlockMiniNode.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "UI/BIM/BIMEditColorPicker.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/BIM/BIMScopeWarning.h"


UBIMDesigner::UBIMDesigner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBIMDesigner::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	Controller = GetOwningPlayer<AEditModelPlayerController>();

	return true;
}

void UBIMDesigner::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBIMDesigner::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DragTick)
	{
		PerformDrag();
	}
}

FReply UBIMDesigner::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == DragKey)
	{
		DragTick = true;
	}
	else
	{
		CheckMouseButtonDownOnBIMDesigner();
	}
	return reply;
}

FReply UBIMDesigner::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply reply = Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
	if (!canvasSlot)
	{
		return reply;
	}
	
	// Get the mouse position relative to widget
	FVector2D localMousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) - canvasSlot->GetPosition();
	FVector2D localMousePosScaled = localMousePos / GetCurrentZoomScale();

	// Predict the mouse position after the scaling is applied 
	float pendingZoomDelta = InMouseEvent.GetWheelDelta() > 0.f ? ZoomDeltaPlus : ZoomDeltaMinus;
	FVector2D localMousePosPostZoom = localMousePos / (GetCurrentZoomScale() / pendingZoomDelta);

	// Apply offset as if location under mouse never moved due to scaling
	FVector2D localZoomOffset = (localMousePosScaled - localMousePosPostZoom) * GetCurrentZoomScale();
	canvasSlot->SetPosition(canvasSlot->GetPosition() + localZoomOffset);

	ScaleBoxForNodes->SetUserSpecifiedScale(GetCurrentZoomScale() * pendingZoomDelta);
	return reply;
}

int32 UBIMDesigner::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FPaintContext Context(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	for (auto& curNode : BIMBlockNodes)
	{
		if (!curNode->IsRootNode && !curNode->ParentID.IsNone())
		{
			UBIMBlockNode *parentNode = IdToNodeMap.FindRef(curNode->ParentID);
			if (parentNode && !curNode->bNodeIsHidden)
			{
				DrawConnectSplineForNodes(Context, parentNode, curNode);
			}
		}
	}
	for (auto& curMiniNode : MiniNodes)
	{
		UBIMBlockNode* node = IdToNodeMap.FindRef(curMiniNode->ParentID);
		if (node)
		{
			DrawConnectSplineForMiniNode(Context, node, curMiniNode);
		}
	}
	return LayerId;
}

void UBIMDesigner::PerformDrag()
{
	float mouseX = 0.f;
	float mouseY = 0.f;
	bool hasMousePosition = Controller->GetMousePosition(mouseX, mouseY);
	FVector2D currentMousePosition = FVector2D(mouseX, mouseY) / UWidgetLayoutLibrary::GetViewportScale(this);

	if (hasMousePosition && !DragReset)
	{
		UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
		if (canvasSlot)
		{
			// Offset node by mouse delta
			FVector2D newCanvasPosition = (currentMousePosition - LastMousePosition) + canvasSlot->GetPosition();
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
	}

}

bool UBIMDesigner::UpdateCraftingAssembly()
{
	return InstancePool.CreateAssemblyFromNodes(Controller->GetDocument()->GetPresetCollection(),
		*GetWorld()->GetAuthGameMode<AEditModelGameMode>()->ObjectDatabase, CraftingAssembly) == EBIMResult::Success;
}

void UBIMDesigner::ToggleCollapseExpandNodes()
{
	bool newAllCollapse = false;
	for (const auto curNode : BIMBlockNodes)
	{
		if (curNode != RootNode && !curNode->NodeCollapse)
		{
			newAllCollapse = true;
			break;
		}
	}
	for (const auto curNode : BIMBlockNodes)
	{
		if (curNode != RootNode)
		{
			curNode->UpdateNodeCollapse(newAllCollapse);
		}
	}
}

void UBIMDesigner::SetNodeAsSelected(const FBIMEditorNodeIDType& InstanceID)
{
	SelectedNodeID = InstanceID;

	// If selected node is root node, then use simpler method to determine highlighted and hidden nodes
	if (RootNode->ID == SelectedNodeID)
	{
		for (const auto curNode : BIMBlockNodes)
		{
			if (curNode == RootNode)
			{
				curNode->UpdateNodeCollapse(false);
				curNode->SetNodeAsHighlighted(true);
				curNode->UpdateNodeHidden(false);
			}
			else
			{
				bool isHighlighted = curNode->ParentID == SelectedNodeID;
				curNode->UpdateNodeCollapse(true);
				curNode->SetNodeAsHighlighted(isHighlighted);
				curNode->UpdateNodeHidden(!isHighlighted);
			}
		}
		AutoArrangeNodes();
		return;
	}

	// Find selected node's children
	TArray<FBIMEditorNodeIDType> selectedNodeChildren;
	const FBIMPresetEditorNodeSharedPtr selectedInst = InstancePool.InstanceFromID(SelectedNodeID);
	if (selectedInst)
	{
		TArray<FBIMPresetEditorNodeSharedPtr> childNodes;
		selectedInst->GatherAllChildNodes(childNodes);
		for (const auto curNode : childNodes)
		{
			selectedNodeChildren.Add(curNode->GetInstanceID());
		}
	}

	// Set nodes expanded or collapsed state based on which node is selected
	// All nodes from selected node's parent lineage are expanded, all other nodes are collapsed
	// All nodes that are within the branch of selected node are consider as highlighted
	TArray<FBIMEditorNodeIDType> selectedNodeLineage;
	InstancePool.FindNodeParentLineage(SelectedNodeID, selectedNodeLineage);
	TArray<FBIMEditorNodeIDType> highlightedNodeIDs;

	for (const auto curNode : BIMBlockNodes)
	{
		bool bIsExpanded = selectedNodeLineage.Contains(curNode->ID) || SelectedNodeID == curNode->ID;
		curNode->UpdateNodeCollapse(!bIsExpanded);

		bool bIsHighlighted = selectedNodeLineage.Contains(curNode->ID) || SelectedNodeID == curNode->ID || selectedNodeChildren.Contains(curNode->ID);
		curNode->SetNodeAsHighlighted(bIsHighlighted);
		if (bIsHighlighted)
		{
			highlightedNodeIDs.Add(curNode->ID);
		}
	}

	// Hide non-highlighted nodes that don't have highlighted siblings
	for (const auto curNode : BIMBlockNodes)
	{
		if (curNode->bNodeHighlight)
		{
			curNode->UpdateNodeHidden(false);
		}
		else
		{
			bool hasHighlightedParent = highlightedNodeIDs.Contains(curNode->ParentID);
			curNode->UpdateNodeHidden(!hasHighlightedParent);
		}
	}

	AutoArrangeNodes();
}

float UBIMDesigner::GetCurrentZoomScale() const
{
	return ScaleBoxForNodes->UserSpecifiedScale;
}

bool UBIMDesigner::EditPresetInBIMDesigner(const FGuid& PresetID)
{
	FBIMPresetEditorNodeSharedPtr rootNode;
	EBIMResult getPresetResult = InstancePool.InitFromPreset(
		Controller->GetDocument()->GetPresetCollection(), 
		*GetWorld()->GetAuthGameMode<AEditModelGameMode>()->ObjectDatabase,		
		PresetID,
		rootNode);
	if (getPresetResult != EBIMResult::Success)
	{
		return false;
	}
	// Since this is a new pool, root node should be the new selected
	SelectedNodeID = rootNode->GetInstanceID();
	UpdateBIMDesigner(true);
	return true;
}

bool UBIMDesigner::SetPresetForNodeInBIMDesigner(const FBIMEditorNodeIDType& InstanceID, const FGuid& PresetID)
{
	EBIMResult result = InstancePool.SetNewPresetForNode(Controller->GetDocument()->GetPresetCollection(), InstanceID, PresetID);
	if (result != EBIMResult::Success)
	{
		return false;
	}
	UpdateCraftingAssembly();
	UpdateBIMDesigner();
	return true;
}

void UBIMDesigner::UpdateBIMDesigner(bool AutoAdjustToRootNode)
{
	FBIMPresetEditorNodeSharedPtr rootNodeInst = InstancePool.GetRootInstance();

	if (!ensureAlways(rootNodeInst.IsValid()))
	{
		return;
	}

	EBIMResult asmResult = InstancePool.CreateAssemblyFromNodes(
		Controller->GetDocument()->GetPresetCollection(),
		*GetWorld()->GetAuthGameMode<AEditModelGameMode>()->ObjectDatabase, CraftingAssembly);

	bool bAssemblyHasPart = false;

	// Should clear focused widget since this function will clear all widgets in BIM Designer
	FSlateApplication::Get().SetAllUserFocusToGameViewport();

	// Should clear any editing menu
	ToggleColorPickerVisibility(false);
	Controller->EditModelUserWidget->ScopeWarningWidget->DismissScopeWarning();

	// Remove and release old nodes
	for (auto& curNodeWidget : BIMBlockNodes)
	{
		// If this node currently has mouse button down, we want to prevent it from 
		// activating NativeOnMouseButtonUp when it is being reused.
		curNodeWidget->ResetMouseButtonOnNode();
		curNodeWidget->RemoveFromParent();
		curNodeWidget->ReleaseNode();
	}

	BIMBlockNodes.Empty();
	IdToNodeMap.Empty();

	TMap<FBIMEditorNodeIDType, class UBIMBlockNode*> IdToNodeMapUnSorted; 	// Temp map used to aid sorting node order

	for (const FBIMPresetEditorNodeSharedPtr& curInstance : InstancePool.GetInstancePool())
	{
		// Determine if this is a node with slots
		TArray<FBIMPresetPartSlot> partSlots;
		curInstance->GetPartSlots(partSlots);
		bool bInstHasParts = partSlots.Num() > 0;
		if (bInstHasParts)
		{
			bAssemblyHasPart = true;
		}

		// Create a normal node or a rigged assembly node, depends on whether any of its children have parts
		TSubclassOf<UBIMBlockNode> nodeWidgetClass = bInstHasParts ? BIMBlockRiggedNodeClass : BIMBlockNodeClass;
		UBIMBlockNode* newBlockNode = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockNode>(nodeWidgetClass);
		if (newBlockNode)
		{
			if (!curInstance->ParentInstance.IsValid()) // assume instance without parent is king node
			{
				RootNode = newBlockNode;
				// Do other kingly things, maybe auto focus when no other node is selected
			}
			newBlockNode->BuildNode(this, curInstance, bInstHasParts);
			BIMBlockNodes.Add(newBlockNode);
			CanvasPanelForNodes->AddChildToCanvas(newBlockNode);
			IdToNodeMapUnSorted.Add(curInstance->GetInstanceID(), newBlockNode);
			UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(newBlockNode);
			if (canvasSlot)
			{
				canvasSlot->SetAutoSize(true);
			}
		}
	}

	// Map nodes by ID
	TArray<FBIMEditorNodeIDType> sortedNodeIDs;
	InstancePool.GetSortedNodeIDs(sortedNodeIDs);
	for (const auto& curID : sortedNodeIDs)
	{
		const auto& curNode = IdToNodeMapUnSorted.FindRef(curID);
		if (curNode)
		{
			IdToNodeMap.Add(curID, curNode);
		}
	}

	SetNodeAsSelected(SelectedNodeID);

	// Adjust canvas size and position to fit root node to screen
	if (AutoAdjustToRootNode)
	{
		UCanvasPanelSlot* rootNodeCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(RootNode);
		UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
		if (RootNode && rootNodeCanvasSlot && canvasSlot)
		{
			ScaleBoxForNodes->SetUserSpecifiedScale(1.f);
			FVector2D rootNodePos = rootNodeCanvasSlot->GetPosition();
			int32 screenX;
			int32 screenY;
			Controller->GetViewportSize(screenX, screenY);
			float newCanvasPosX = RootNodeHorizontalPosition;
			float newCanvasPosY = (static_cast<float>(screenY) * 0.5f) - rootNodePos.Y - (RootNode->GetEstimatedNodeSize().Y * 0.5f);
			canvasSlot->SetPosition(FVector2D(newCanvasPosX, newCanvasPosY));
		}
	}
}

void UBIMDesigner::AutoArrangeNodes()
{
	for (auto& curItem : NodesWithAddLayerButton)
	{
		auto& curAddButton = curItem.Value;
		curAddButton->RemoveFromParent();
		Controller->HUDDrawWidget->UserWidgetPool.Release(curAddButton);
	}
	NodesWithAddLayerButton.Empty();
	for (const FBIMPresetEditorNodeSharedPtr& curInstance : InstancePool.GetInstancePool())
	{
		if (curInstance->bWantAddButton)
		{
			UBIMBlockNode* nodeWithAddButton = IdToNodeMap.FindRef(curInstance->GetInstanceID());
			if (nodeWithAddButton && !nodeWithAddButton->bNodeIsHidden)
			{
				UBIMBlockAddLayer* newAddButton = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockAddLayer>(BIMAddLayerClass);
				if (newAddButton)
				{
					NodesWithAddLayerButton.Add(nodeWithAddButton, newAddButton);

					newAddButton->ParentID = curInstance->ParentInstance.Pin()->GetInstanceID();
					newAddButton->PresetID = curInstance->WorkingPresetCopy.GUID;
					newAddButton->ParentSetIndex = curInstance->MyParentPinSetIndex;

					CanvasPanelForNodes->AddChildToCanvas(newAddButton);
					UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(newAddButton);
					if (canvasSlot)
					{
						canvasSlot->SetAutoSize(true);
					}
				}
			}
		}
	}

	NodeCoordinateMap.Empty();

	if (BIMBlockNodes.Num() == 0)
	{
		return;
	}

	TMap<class UBIMBlockNode*, int32> nodeColumnMap;
	TMap<class UBIMBlockNode*, int32> nodeRowMap;
	TMap<int32, float> columnWidthMap;

	// Assign nodes to columns
	for (auto& curItem : IdToNodeMap)
	{
		UBIMBlockNode *curNodeToSearch = curItem.Value;
		bool searchingColumn = true;
		int32 column = 0;
		while (searchingColumn)
		{
			if (curNodeToSearch->IsRootNode)
			{
				searchingColumn = false;
			}
			else
			{
				curNodeToSearch = IdToNodeMap.FindRef(curNodeToSearch->ParentID);
				if (curNodeToSearch)
				{
					column++;
				}
				else
				{
					searchingColumn = false;
				}
			}
		}
		nodeColumnMap.Add(curItem.Value, column);
		// Assign column width based on the node's width
		float curNodeWidth = curItem.Value->GetEstimatedNodeSize().X;
		float curColumnWidth = columnWidthMap.FindRef(column);
		columnWidthMap.Add(column, FMath::Max(curColumnWidth, curNodeWidth));
	}

	// Assign nodes to row and NodeCoordinateMap(in rows and columns)
	// Ex: KingNode should be (0, 0), node in 3rd column, 2nd row should be (2, 1)
	for (auto& curItem : nodeColumnMap)
	{
		UBIMBlockNode *curNode = curItem.Key;
		int32 row = 0;
		bool searchingRow = true;
		while (searchingRow)
		{
			int32 columnID = nodeColumnMap.FindRef(curNode);
			if (columnID > -1)
			{
				UBIMBlockNode *coordNode = NodeCoordinateMap.FindRef(FIntPoint(columnID, row));
				if (coordNode)
				{
					row++;
				}
				else
				{
					NodeCoordinateMap.Add(FIntPoint(columnID, row), curNode);
					nodeRowMap.Add(curNode, row);
					searchingRow = false;
				}
			}
		}
	}

	// Find screen coordinate for node
	TMap<class UBIMBlockNode*, FVector2D> screenPosNodeMap;
	TMap<int32, float> maxColumnHeightMap;
	float maxCanvasHeight = 0.f;
	float maxCanvasHeightBottomEdge = 0.f;

	for (auto& curItem : IdToNodeMap)
	{
		UBIMBlockNode *curNode = curItem.Value;
		int32 mapColumn = nodeColumnMap.FindRef(curNode);
		int32 mapRow = nodeRowMap.FindRef(curNode);

		if (mapColumn > -1 && mapRow > -1)
		{
			FIntPoint curXY = FIntPoint(mapColumn, mapRow);
			float curNodeY = 0.f;
			int32 curRow = 0;
			while (curRow != mapRow)
			{
				UBIMBlockNode *curNodeToEstimate = NodeCoordinateMap.FindRef(FIntPoint(curXY.X, curRow));
				if (curNodeToEstimate)
				{
					FVector2D estimatedSize = curNodeToEstimate->GetEstimatedNodeSize();
					curNodeY = curNodeY + estimatedSize.Y + NodeVerticalSpacing; // padding
					if (NodesWithAddLayerButton.Contains(curNodeToEstimate))
					{
						curNodeY += AddButtonSpacingBetweenNodes;
					}
					curRow++;
				}
			}
			// Find x position by column width
			float posX = 0.f;
			for (int32 i = 0; i < mapColumn; ++i)
			{
				posX += columnWidthMap.FindRef(i) + NodeHorizontalSpacing;
			}
			screenPosNodeMap.Add(curNode, FVector2D(posX, curNodeY));
			if (curNodeY > maxCanvasHeight || FMath::IsNearlyEqual(curNodeY, maxCanvasHeight))
			{
				maxCanvasHeight = curNodeY;
				maxCanvasHeightBottomEdge = curNode->GetEstimatedNodeSize().Y + curNodeY;
				if (NodesWithAddLayerButton.Contains(curNode))
				{
					maxCanvasHeightBottomEdge += AddButtonSpacingBetweenNodes;
				}
			}

			float curMaxColumnHeight = maxColumnHeightMap.FindRef(mapColumn);
			if (curNodeY > curMaxColumnHeight || FMath::IsNearlyEqual(curNodeY, curMaxColumnHeight))
			{
				maxColumnHeightMap.Add(mapColumn, curNodeY);
			}
		}
	}

	// Set nodes position
	for (auto& curItem : IdToNodeMap)
	{
		UBIMBlockNode *curNode = curItem.Value;
		FVector2D curPos = screenPosNodeMap.FindRef(curNode);
		UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(curNode);
		if (canvasSlot)
		{
			if (curNode->IsRootNode)
			{
				float kingNodeAdjustPos = (maxCanvasHeightBottomEdge / 2.f) - (curNode->GetEstimatedNodeSize().Y / 2.f);
				canvasSlot->SetPosition(FVector2D(curPos.X, kingNodeAdjustPos));
			}
			else
			{
				int32 curColumn = nodeColumnMap.FindRef(curNode);
				float curColumnMax = maxColumnHeightMap.FindRef(curColumn);
				float columnStartingHeight = (maxCanvasHeight - curColumnMax) / 2.f;
				canvasSlot->SetPosition(FVector2D(curPos.X, curPos.Y + columnStartingHeight));
			}
		}
	}

	// Set AddButtons position
	for (auto& curItem : NodesWithAddLayerButton)
	{
		UBIMBlockNode *nodeToAdd = curItem.Key;
		UCanvasPanelSlot* nodeToAddCanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(nodeToAdd);
		if (nodeToAddCanvasSlot)
		{
			FVector2D addButtonPosition = nodeToAddCanvasSlot->GetPosition() + FVector2D(0.f, nodeToAdd->GetEstimatedNodeSize().Y + AddButtonGapFromNode);

			UBIMBlockAddLayer *addButton = curItem.Value;
			UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(addButton);
			if (canvasSlot)
			{
				canvasSlot->SetPosition(addButtonPosition);
			}
		}
	}

	// Mini nodes represent children that are hidden, but their parent node is visible
	for (auto& curMiniNode : MiniNodes)
	{
		curMiniNode->RemoveFromParent();
		Controller->HUDDrawWidget->UserWidgetPool.Release(curMiniNode);
	}
	MiniNodes.Empty();

	for (auto& curNode : BIMBlockNodes)
	{
		if (!curNode->bNodeIsHidden)
		{
			FBIMPresetEditorNodeSharedPtr instPtr = InstancePool.InstanceFromID(curNode->ID);
			if (instPtr.IsValid())
			{
				TArray<FBIMEditorNodeIDType> rawChildrenIDs; TArray<FBIMEditorNodeIDType> childrenIDs;
				instPtr->GatherChildrenInOrder(rawChildrenIDs);

				// Only check nodes that has widgets
				for (auto curID : rawChildrenIDs)
				{
					if (IdToNodeMap.FindRef(curID))
					{
						childrenIDs.Add(curID);
					}
				}

				curNode->ToggleConnectorVisibilityToChildren(childrenIDs.Num() > 0);

				if (childrenIDs.Num() > 0)
				{
					UBIMBlockNode* childNode = IdToNodeMap.FindRef(childrenIDs[0]);
					if (childNode && childNode->bNodeIsHidden)
					{
						UBIMBlockMiniNode* newMiniNode = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockMiniNode>(BIMMiniNodeClass);
						if (newMiniNode)
						{
							CanvasPanelForNodes->AddChildToCanvas(newMiniNode);
							newMiniNode->BuildMiniNode(this, instPtr->GetInstanceID(), childrenIDs.Num());
							MiniNodes.Add(newMiniNode);

							// Place miniNode in average position of the children nodes
							FVector2D miniNodePosition = FVector2D::ZeroVector;
							for (int32 i = 0; i < childrenIDs.Num(); ++i)
							{
								UBIMBlockNode* curChildNode = IdToNodeMap.FindRef(childrenIDs[i]);
								UCanvasPanelSlot* curChildSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(curChildNode);
								if (curChildSlot)
								{
									miniNodePosition += curChildSlot->GetPosition();
								}
							}
							miniNodePosition = miniNodePosition / childrenIDs.Num();

							UCanvasPanelSlot* miniNodeSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(newMiniNode);
							if (miniNodeSlot)
							{
								miniNodeSlot->SetPosition(miniNodePosition);
							}
						}
					}
				}
			}
		}
	}
}

void UBIMDesigner::DrawConnectSplineForNodes(const FPaintContext& context, class UBIMBlockNode* StartNode, class UBIMBlockNode* EndNode) const
{
	UCanvasPanelSlot* canvasSlotStart = UWidgetLayoutLibrary::SlotAsCanvasSlot(StartNode);
	UCanvasPanelSlot* canvasSlotEnd = UWidgetLayoutLibrary::SlotAsCanvasSlot(EndNode);
	UCanvasPanelSlot* canvasSlotScaleBox = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
	if (!(canvasSlotStart && canvasSlotEnd && canvasSlotScaleBox))
	{
		return;
	}

	FVector2D startNodeSize = StartNode->GetEstimatedNodeSize();
	FVector2D endNodeSize = EndNode->GetEstimatedNodeSize();

	FVector2D startNodePos = canvasSlotStart->GetPosition() + FVector2D(startNodeSize.X, startNodeSize.Y / 2.f);
	// If there's a slot item connect to the node, use slot item's position instead of node's mid position
	if (StartNode->bNodeHasSlotPart && !StartNode->NodeCollapse)
	{
		UBIMBlockSlotListItem* slotListItem = StartNode->BIMBlockSlotList->NodeIDToSlotMapItem.FindRef(EndNode->ID);
		if (slotListItem)
		{
			float slotOffset = SlotListStartOffset;
			if (StartNode->NodeDirty)
			{
				slotOffset += SlotListDirtyTabSize;
			}
			startNodePos.Y = canvasSlotStart->GetPosition().Y + slotOffset + (slotListItem->SlotIndex * SlotListItemHeight);
		}
	}
	FVector2D endNodePos = canvasSlotEnd->GetPosition() + FVector2D(0.f, endNodeSize.Y / 2.f);

	FVector2D startPoint = (startNodePos * GetCurrentZoomScale()) + canvasSlotScaleBox->GetPosition();
	FVector2D endPoint = (endNodePos * GetCurrentZoomScale()) + canvasSlotScaleBox->GetPosition();

	float startX = startPoint.X + NodeSplineBezierStartPercentage * (endPoint.X - startPoint.X);
	float endX = startPoint.X + NodeSplineBezierEndPercentage * (endPoint.X - startPoint.X);

	TArray<FVector2D> splinePts = { startPoint, FVector2D(startX, startPoint.Y), FVector2D(endX, endPoint.Y), endPoint };

	UModumateSlateHelper::DrawCubicBezierSplineBP(context, splinePts, StartNode->bNodeHighlight ? NodeSplineHighlightedColor : NodeSplineFadeColor, NodeSplineThickness);
}

void UBIMDesigner::DrawConnectSplineForMiniNode(const FPaintContext& context, class UBIMBlockNode* StartNode, class UBIMBlockMiniNode* MiniNode) const
{
	UCanvasPanelSlot* canvasSlotStart = UWidgetLayoutLibrary::SlotAsCanvasSlot(StartNode);
	UCanvasPanelSlot* canvasSlotEnd = UWidgetLayoutLibrary::SlotAsCanvasSlot(MiniNode);
	UCanvasPanelSlot* canvasSlotScaleBox = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
	if (!(canvasSlotStart && canvasSlotEnd && canvasSlotScaleBox))
	{
		return;
	}

	FVector2D startNodeSize = StartNode->GetEstimatedNodeSize();
	FVector2D startNodePos = canvasSlotStart->GetPosition() + FVector2D(startNodeSize.X, startNodeSize.Y / 2.f);
	FVector2D endNodePos = canvasSlotEnd->GetPosition() + FVector2D(0.f, MiniNode->AttachmentOffset);

	FVector2D startPoint = (startNodePos * GetCurrentZoomScale()) + canvasSlotScaleBox->GetPosition();
	FVector2D endPoint = (endNodePos * GetCurrentZoomScale()) + canvasSlotScaleBox->GetPosition();

	float startX = startPoint.X + NodeSplineBezierStartPercentage * (endPoint.X - startPoint.X);
	float endX = startPoint.X + NodeSplineBezierEndPercentage * (endPoint.X - startPoint.X);

	TArray<FVector2D> splinePts = { startPoint, FVector2D(startX, startPoint.Y), FVector2D(endX, endPoint.Y), endPoint };

	UModumateSlateHelper::DrawCubicBezierSplineBP(context, splinePts, StartNode->bNodeHighlight ? NodeSplineHighlightedColor : NodeSplineFadeColor, NodeSplineThickness);
}

FGuid UBIMDesigner::GetPresetID(const FBIMEditorNodeIDType& InstanceID)
{
	FBIMPresetEditorNodeSharedPtr instPtr = InstancePool.InstanceFromID(InstanceID);
	if (ensureAlways(instPtr.IsValid()))
	{
		return instPtr->WorkingPresetCopy.GUID;
	}
	return FGuid();
}

bool UBIMDesigner::DeleteNode(const FBIMEditorNodeIDType& InstanceID)
{
	TArray<FBIMEditorNodeIDType> outDestroyed;
	EBIMResult result = InstancePool.DestroyNodeInstance(InstanceID, outDestroyed);
	if (result != EBIMResult::Success)
	{
		return false;
	}
	UpdateCraftingAssembly();
	UpdateBIMDesigner();
	SetNodeAsSelected(RootNode->ID);
	return true;
}

bool UBIMDesigner::AddNodeFromPreset(const FBIMEditorNodeIDType& ParentID, const FGuid& PresetID, int32 ParentSetIndex)
{
	FBIMPresetEditorNodeSharedPtr newNode = InstancePool.CreateNodeInstanceFromPreset(
		Controller->GetDocument()->GetPresetCollection(),
		ParentID, PresetID, ParentSetIndex, INDEX_NONE);
	if (!newNode.IsValid())
	{
		return false;
	}
	UpdateCraftingAssembly();
	UpdateBIMDesigner();
	return true;
}

bool UBIMDesigner::ApplyBIMFormElement(const FBIMEditorNodeIDType& NodeID, const FBIMPresetFormElement& FormElement)
{
	FBIMPresetEditorNodeSharedPtr instPtr = InstancePool.InstanceFromID(NodeID);
	if (!instPtr.IsValid())
	{
		return false;
	}

	// TODO: return delta added to undo/redo stack
	FBIMPresetEditorDelta delta;
	if (ensureAlways(instPtr->WorkingPresetCopy.MakeDeltaForFormElement(FormElement, delta) == EBIMResult::Success))
	{
		instPtr->WorkingPresetCopy.ApplyDelta(*GetWorld()->GetAuthGameMode<AEditModelGameMode>()->ObjectDatabase,delta);
	}

	UpdateCraftingAssembly();
	UpdateBIMDesigner();

	return true;
}

bool UBIMDesigner::GetNodeForReorder(const FVector2D &OriginalNodeCanvasPosition, const FBIMEditorNodeIDType& NodeID)
{
	// Get the mouse position relative to BIM Designer current zoom and offset
	UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ScaleBoxForNodes);
	if (!canvasSlot)
	{
		return false;
	}
	FVector2D localMousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(this) - canvasSlot->GetPosition();
	FVector2D localMousePosScaled = localMousePos / GetCurrentZoomScale();

	// Groups of nodes to check for reorder. Assuming nodes in canvas already arranged vertically correctly
	FBIMPresetEditorNodeSharedPtr instPtr = InstancePool.InstanceFromID(NodeID);
	if (!instPtr.IsValid())
	{
		return false;
	}
	TArray<UBIMBlockNode*> nodeGroup;
	TArray<FBIMEditorNodeIDType> relatives;
	if (instPtr->FindOtherChildrenOnPin(relatives) == EBIMResult::Success)
	{
		for (auto& curID : relatives)
		{
			UBIMBlockNode *nodeToAdd = IdToNodeMap.FindRef(curID);
			if (nodeToAdd)
			{
				nodeGroup.Add(nodeToAdd);
			}
		}
	}

	// Check if reordering is possible
	UBIMBlockNode *reorderFromNode = IdToNodeMap.FindRef(NodeID);
	if (!(reorderFromNode && nodeGroup.Num() > 1))
	{
		return false;
	}
	int32 fromOrder = INDEX_NONE;
	int32 toOrder = 0; // If mouse position is lower than any node positions, this will remain 0
	for (int32 order = 0; order < nodeGroup.Num(); ++order)
	{
		FVector2D nodePosition = FVector2D::ZeroVector;
		// If this node is to be reordered, use its original position instead of current position
		if (reorderFromNode == nodeGroup[order])
		{
			fromOrder = order;
			nodePosition = OriginalNodeCanvasPosition;
		}
		// Other nodes can use their current position
		else
		{
			UCanvasPanelSlot* curSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(nodeGroup[order]);
			if (curSlot)
			{
				nodePosition = curSlot->GetPosition();
			}
		}
		// Check node's position against mouse location
		if (localMousePosScaled.Y > nodePosition.Y)
		{
			toOrder = order;
		}
	}
	// Don't reorder if position remains the same
	EBIMResult result = EBIMResult::None;
	if ((fromOrder != toOrder) && (fromOrder != INDEX_NONE))
	{
		result = InstancePool.ReorderChildNode(NodeID, fromOrder, toOrder);
		if (result == EBIMResult::Success)
		{
			UpdateCraftingAssembly();
			UpdateBIMDesigner();
			return true;
		}
	}
	AutoArrangeNodes();
	return false;
}

bool UBIMDesigner::SavePresetFromNode(bool SaveAs, const FBIMEditorNodeIDType& InstanceID)
{
	FBIMPresetEditorNodeSharedPtr node = InstancePool.InstanceFromID(InstanceID);
	if (!ensureAlways(node.IsValid()))
	{
		return false;
	}

	// Update its DisplayName from property
	FText presetDisplayName;
	if (node->WorkingPresetCopy.TryGetProperty(BIMPropertyNames::Name, presetDisplayName))
	{
		node->WorkingPresetCopy.DisplayName = presetDisplayName;
	}

	SavePendingPreset = node->WorkingPresetCopy;

	SavePendingPreset.Edited = true;

	if (SaveAs)
	{
		// Creating new preset, do not check for affected presets
		SavePendingPreset.GUID.Invalidate();
		TSharedPtr<FBIMPresetDelta> presetDelta = Controller->GetDocument()->GetPresetCollection().MakeCreateNewDelta(SavePendingPreset);
		Controller->GetDocument()->ApplyDeltas({presetDelta},GetWorld());

		if (!node->ParentInstance.IsValid())
		{
			CraftingAssembly.PresetGUID = SavePendingPreset.GUID;
		}
		else
		{
			FBIMPresetEditorNodeSharedPtr parent = node->ParentInstance.Pin();

			if (node->MyParentPartSlot.IsValid())
			{
				TArray<FBIMPresetPartSlot> partSlots;
				parent->GetPartSlots(partSlots);

				for (int32 i = 0; i < partSlots.Num(); ++i)
				{
					if (partSlots[i].SlotPresetGUID == node->MyParentPartSlot)
					{
						InstancePool.SetPartPreset(Controller->GetDocument()->GetPresetCollection(),parent->GetInstanceID(),i, SavePendingPreset.GUID);
					}
				}
			}
			else
			{
				InstancePool.SetNewPresetForNode(Controller->GetDocument()->GetPresetCollection(), node->GetInstanceID(), SavePendingPreset.GUID);
			}
		}
		node->WorkingPresetCopy = SavePendingPreset;
		UModumateAnalyticsStatics::RecordPresetCreation(this);

		node->OriginalPresetCopy = node->WorkingPresetCopy;

		SelectedNodeID = InstanceID;
		UpdateBIMDesigner();
		return true;
	}
	else
	{
		// Check if there are presets being affected by this change
		SavePendingInstanceID = InstanceID;
		TArray<FGuid> affectedGUID;
		Controller->GetDocument()->GetPresetCollection().GetAllAncestorPresets(SavePendingPreset.GUID, affectedGUID);
		if (affectedGUID.Num() > 0)
		{
			// Use scope warning message if there is at least one affected preset
			Controller->EditModelUserWidget->ScopeWarningWidget->BuildScopeWarning(affectedGUID);
			return false;
		}
		else
		{
			return ConfirmSavePendingPreset();
		}
	}
}

bool UBIMDesigner::ConfirmSavePendingPreset()
{
	TSharedPtr<FBIMPresetDelta> presetDelta = Controller->GetDocument()->GetPresetCollection().MakeUpdateDelta(SavePendingPreset);
	Controller->GetDocument()->ApplyDeltas({ presetDelta }, GetWorld());
	UModumateAnalyticsStatics::RecordPresetUpdate(this);

	FBIMPresetEditorNodeSharedPtr node = InstancePool.InstanceFromID(SavePendingInstanceID);
	if (!ensureAlways(node.IsValid()))
	{
		return false;
	}

	node->OriginalPresetCopy = node->WorkingPresetCopy;

	SelectedNodeID = SavePendingInstanceID;
	UpdateBIMDesigner();
	return true;
}

void UBIMDesigner::ToggleSlotNode(const FBIMEditorNodeIDType& ParentID, int32 SlotID, bool NewEnable)
{
	const FBIMPresetEditorNodeSharedPtr nodeParent = InstancePool.InstanceFromID(ParentID);
	if (nodeParent.IsValid())
	{
		EBIMResult result = EBIMResult::Error;
		if (NewEnable)
		{
			// TODO: get default preset for slot
			FGuid newPartPreset = nodeParent->OriginalPresetCopy.PartSlots[SlotID].PartPresetGUID;
			if (!newPartPreset.IsValid())
			{
				TArray<FGuid> swapPresets;
				Controller->GetDocument()->GetPresetCollection().GetPresetsForSlot(nodeParent->OriginalPresetCopy.PartSlots[SlotID].SlotPresetGUID, swapPresets);
				if (swapPresets.Num() > 0)
				{
					newPartPreset = swapPresets[0];
				}
			}
			if (newPartPreset.IsValid())
			{
				result = InstancePool.SetPartPreset(Controller->GetDocument()->GetPresetCollection(), ParentID, SlotID, newPartPreset);
			}
		}
		else
		{
			result = InstancePool.ClearPartPreset(ParentID, SlotID);
		}

		if (result == EBIMResult::Success)
		{
			UpdateBIMDesigner();
		}
	}
}

void UBIMDesigner::CheckMouseButtonDownOnBIMDesigner()
{
	// Close Color Picker widget if mouse is clicked out of its geometry
	if (BIM_EditColorBP->IsVisible() && !BIM_EditColorBP->GetTickSpaceGeometry().IsUnderLocation(UWidgetLayoutLibrary::GetMousePositionOnPlatform()))
	{
		ToggleColorPickerVisibility(false);
	}
}

void UBIMDesigner::ToggleColorPickerVisibility(bool NewVisibility)
{
	BIM_EditColorBP->SetVisibility(NewVisibility ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
