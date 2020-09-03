// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "UI/BIM/BIMDesigner.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/CanvasPanel.h"
#include "UI/BIM/BIMBlockNode.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "UI/EditModelPlayerHUD.h"
#include "ModumateCore/ModumateSlateHelper.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "UI/BIM/BIMBlockAddLayer.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "BIMKernel/BIMAssemblySpec.h"

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

	Controller = GetOwningPlayer<AEditModelPlayerController_CPP>();

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
		if (!curNode->IsKingNode && curNode->ParentID > -1)
		{
			UBIMBlockNode *parentNode = IdToNodeMap.FindRef(curNode->ParentID);
			if (parentNode)
			{
				DrawConnectSplineForNodes(Context, parentNode, curNode);
			}
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

float UBIMDesigner::GetCurrentZoomScale() const
{
	return ScaleBoxForNodes->UserSpecifiedScale;
}

bool UBIMDesigner::EditPresetInBIMDesigner(const FName& PresetID)
{
	FBIMCraftingTreeNodeSharedPtr rootNode;
	ECraftingResult getPresetResult = InstancePool.InitFromPreset(Controller->GetDocument()->PresetManager.CraftingNodePresets, PresetID, rootNode);
	if (getPresetResult != ECraftingResult::Success)
	{
		return false;
	}
	UpdateBIMDesigner();
	return true;
}

bool UBIMDesigner::SetPresetForNodeInBIMDesigner(int32 InstanceID, const FName &PresetID)
{
	ECraftingResult result = InstancePool.SetNewPresetForNode(Controller->GetDocument()->PresetManager.CraftingNodePresets, InstanceID, PresetID);
	if (result != ECraftingResult::Success)
	{
		return false;
	}

	UpdateBIMDesigner();
	return true;
}

void UBIMDesigner::UpdateBIMDesigner()
{
	ECraftingResult asmResult = InstancePool.CreateAssemblyFromNodes(
		Controller->GetDocument()->PresetManager.CraftingNodePresets,
		*GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>()->ObjectDatabase, CraftingAssembly);

	CanvasPanelForNodes->ClearChildren();
	BIMBlockNodes.Empty();
	IdToNodeMap.Empty();
	NodesWithAddLayerButton.Empty();

	// Child groups that allow user to add more nodes
	TArray<const FBIMCraftingTreeNode::FAttachedChildGroup*> AddableChildGroup;

	for (const FBIMCraftingTreeNodeSharedPtr& curInstance : InstancePool.GetInstancePool())
	{
		UBIMBlockNode *newBlockNode = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockNode>(BIMBlockNodeClass);
		if (newBlockNode)
		{
			if (!curInstance->ParentInstance.IsValid()) // assume instance without parent is king node
			{
				newBlockNode->IsKingNode = true;
				// Do other kingly things, maybe auto focus when no other node is selected
			}
			newBlockNode->BuildNode(this, curInstance);
			BIMBlockNodes.Add(newBlockNode);
			CanvasPanelForNodes->AddChildToCanvas(newBlockNode);
			IdToNodeMap.Add(curInstance->GetInstanceID(), newBlockNode);
			UCanvasPanelSlot* canvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(newBlockNode);
			if (canvasSlot)
			{
				canvasSlot->SetAutoSize(true);
			}

			// Save the node's child group if it can add child
			for (auto& curChildGroup : curInstance->AttachedChildren)
			{
				if (curChildGroup.SetType.MaxCount > curChildGroup.Children.Num() || 
					curChildGroup.SetType.MaxCount == -1)
				{
					AddableChildGroup.AddUnique(&curChildGroup);
				}
			}
		}
	}

	for (int32 i = 0; i < AddableChildGroup.Num(); ++i)
	{
		// Place add button under the last attached child node
		// TODO: Possible tech debt. This is under the assumption that addable node already has at least one child node attached
		if (AddableChildGroup[i]->Children.Num() > 0)
		{
			int32 lastChildID = AddableChildGroup[i]->Children.Last().Pin()->GetInstanceID();
			UBIMBlockNode *nodeWithAddButton = IdToNodeMap.FindRef(lastChildID);
			if (nodeWithAddButton)
			{
				UBIMBlockAddLayer *newAddButton = Controller->GetEditModelHUD()->GetOrCreateWidgetInstance<UBIMBlockAddLayer>(BIMAddLayerClass);
				if (newAddButton)
				{
					NodesWithAddLayerButton.Add(nodeWithAddButton, newAddButton);

					newAddButton->ParentID = AddableChildGroup[i]->Children.Last().Pin()->ParentInstance.Pin()->GetInstanceID();
					newAddButton->PresetID = AddableChildGroup[i]->Children.Last().Pin()->PresetID;
					newAddButton->ParentSetIndex = i;
					newAddButton->ParentSetPosition = AddableChildGroup[i]->Children.Num();

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

	AutoArrangeNodes();
}

void UBIMDesigner::AutoArrangeNodes()
{
	NodeCoordinateMap.Empty();

	if (BIMBlockNodes.Num() == 0)
	{
		return;
	}

	TMap<class UBIMBlockNode*, int32> nodeColumnMap;
	TMap<class UBIMBlockNode*, int32> nodeRowMap;

	// Assign nodes to columns
	for (auto& curItem : IdToNodeMap)
	{
		UBIMBlockNode *curNodeToSearch = curItem.Value;
		bool searchingColumn = true;
		int32 column = 0;
		while (searchingColumn)
		{
			if (curNodeToSearch->ID == 0)
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
			screenPosNodeMap.Add(curNode, FVector2D(mapColumn * NodeHorizontalSpacing, curNodeY));
			if (curNodeY > maxCanvasHeight)
			{
				maxCanvasHeight = curNodeY;
				maxCanvasHeightBottomEdge = curNode->GetEstimatedNodeSize().Y + curNodeY;
				if (NodesWithAddLayerButton.Contains(curNode))
				{
					maxCanvasHeightBottomEdge += AddButtonSpacingBetweenNodes;
				}
			}

			float curMaxColumnHeight = maxColumnHeightMap.FindRef(mapColumn);
			if (curNodeY > curMaxColumnHeight)
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
			if (curNode->IsKingNode)
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
	FVector2D endNodePos = canvasSlotEnd->GetPosition() + FVector2D(0.f, endNodeSize.Y / 2.f);

	FVector2D startPoint = (startNodePos * GetCurrentZoomScale()) + canvasSlotScaleBox->GetPosition();
	FVector2D endPoint = (endNodePos * GetCurrentZoomScale()) + canvasSlotScaleBox->GetPosition();

	float startX = startPoint.X + NodeSplineBezierStartPercentage * (endPoint.X - startPoint.X);
	float endX = startPoint.X + NodeSplineBezierEndPercentage * (endPoint.X - startPoint.X);

	TArray<FVector2D> splinePts = { startPoint, FVector2D(startX, startPoint.Y), FVector2D(endX, endPoint.Y), endPoint };

	UModumateSlateHelper::DrawCubicBezierSplineBP(context, splinePts, NodeSplineColor, NodeSplineThickness);
}

FName UBIMDesigner::GetPresetID(int32 InstanceID)
{
	FBIMCraftingTreeNodeSharedPtr instPtr = InstancePool.InstanceFromID(InstanceID);
	if (ensureAlways(instPtr.IsValid()))
	{
		return instPtr->PresetID;
	}
	return NAME_None;
}

bool UBIMDesigner::DeleteNode(int32 InstanceID)
{
	TArray<int32> outDestroyed;
	ECraftingResult result = InstancePool.DestroyNodeInstance(InstanceID, outDestroyed);
	if (result != ECraftingResult::Success)
	{
		return false;
	}
	UpdateBIMDesigner();
	return true;
}

bool UBIMDesigner::AddNodeFromPreset(int32 ParentID, const FName& PresetID, int32 ParentSetIndex, int32 ParentSetPosition)
{
	FBIMCraftingTreeNodeSharedPtr newNode = InstancePool.CreateNodeInstanceFromPreset(
		Controller->GetDocument()->PresetManager.CraftingNodePresets,
		ParentID, PresetID, ParentSetIndex, ParentSetPosition);
	if (!newNode.IsValid())
	{
		return false;
	}
	UpdateBIMDesigner();
	return true;
}

bool UBIMDesigner::SetNodeProperty(int32 NodeID, const EBIMValueScope &Scope, const FBIMNameType &NameType, const FString &Value)
{
	FBIMCraftingTreeNodeSharedPtr instPtr = InstancePool.InstanceFromID(NodeID);
	if (!instPtr.IsValid())
	{
		return false;
	}
	instPtr->InstanceProperties.SetProperty(Scope, NameType, Value);
	UpdateBIMDesigner();
	return true;
}
