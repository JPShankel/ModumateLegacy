// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Core/BIMKey.h"

#include "BIMDesigner.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMDesigner : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMDesigner(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	bool DragTick = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	bool DragReset = true;

	UPROPERTY()
	class UBIMBlockNode* RootNode;

	UPROPERTY()
	TArray<class UBIMBlockNode*> BIMBlockNodes;

	UPROPERTY()
	TMap<int32, class UBIMBlockNode*> IdToNodeMap;

	UPROPERTY()
	TMap<FIntPoint, class UBIMBlockNode*> NodeCoordinateMap;

	UPROPERTY()
	TMap<class UBIMBlockNode*, class UBIMBlockAddLayer*> NodesWithAddLayerButton;

	UPROPERTY()
	TArray<class UBIMBlockMiniNode*> MiniNodes;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey DragKey = EKeys::MiddleMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomDeltaPlus = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomDeltaMinus = 0.91f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor NodeSplineHighlightedColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor NodeSplineFadeColor = FLinearColor(0.25f, 0.25f, 0.25f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeSplineThickness= 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeSplineBezierStartPercentage = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeSplineBezierEndPercentage = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeHorizontalSpacing = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeVerticalSpacing = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RootNodeHorizontalPosition = 500.f;

	// Spacing for Add layer button under this node, if adding is allowed from its parent node
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AddButtonSpacingBetweenNodes = 60.f;

	// The gap between this node and the add button, if adding is allowed from its parent node
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AddButtonGapFromNode = 10.f;

	// Starting position for the first slot item, offset from top of the node
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlotListStartOffset = 52.f;

	// Size of the additional tab if this node is dirty
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlotListDirtyTabSize = 100.f;

	// Height of slot item
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlotListItemHeight = 18.f;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UScaleBox *ScaleBoxForNodes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCanvasPanel *CanvasPanelForNodes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USizeBox *SizeBoxSwapTray;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockAssembliesList *SelectionTray_Block_Swap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockNode> BIMBlockNodeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockNode> BIMBlockRiggedNodeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockAddLayer> BIMAddLayerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockMiniNode> BIMMiniNodeClass;

	UFUNCTION()
	void PerformDrag();

	int32 SelectedNodeID = INDEX_NONE;
	FBIMPresetEditor InstancePool;
	FBIMAssemblySpec CraftingAssembly;

	bool UpdateCraftingAssembly();
	void ToggleCollapseExpandNodes();
	void SetNodeAsSelected(int32 InstanceID);
	float GetCurrentZoomScale() const;
	bool EditPresetInBIMDesigner(const FBIMKey& PresetID);
	bool SetPresetForNodeInBIMDesigner(int32 InstanceID, const FBIMKey& PresetID);
	void UpdateBIMDesigner(bool AutoAdjustToRootNode = false);
	void AutoArrangeNodes();
	void DrawConnectSplineForNodes(const FPaintContext& context, class UBIMBlockNode* StartNode, class UBIMBlockNode* EndNode) const;
	void DrawConnectSplineForMiniNode(const FPaintContext& context, class UBIMBlockNode* StartNode, class UBIMBlockMiniNode* MiniNode) const;
	FBIMKey GetPresetID(int32 InstanceID);
	bool DeleteNode(int32 InstanceID);
	bool AddNodeFromPreset(int32 ParentID, const FBIMKey& PresetID, int32 ParentSetIndex, int32 ParentSetPosition);
	bool SetNodeProperty(int32 NodeID, const EBIMValueScope &Scope, const FBIMNameType &NameType, const FString &Value);
	bool UpdateNodeSwapMenuVisibility(int32 SwapFromNodeID, bool NewVisibility, FVector2D offset = FVector2D::ZeroVector);
	bool GetNodeForReorder(const FVector2D &OriginalNodeCanvasPosition, int32 NodeID);
	bool SavePresetFromNode(bool SaveAs, int32 InstanceID);
	void ToggleSlotNode(int32 ParentPartSlotID, int32 SlotID, bool NewEnable);
};
