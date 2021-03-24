// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"

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

	/*
	* The BIM designer uses a cached copy of the preset collection from the document so it can generate icons for uncommitted changes
	*/
	void UpdateCachedPresetCollection();

	UPROPERTY()
	class UBIMBlockNode* RootNode;

	UPROPERTY()
	TArray<class UBIMBlockNode*> BIMBlockNodes;

	UPROPERTY()
	TMap<FIntPoint, class UBIMBlockNode*> NodeCoordinateMap;

	UPROPERTY()
	TMap<class UBIMBlockNode*, class UBIMBlockAddLayer*> NodesWithAddLayerButton;

	UPROPERTY()
	TArray<class UBIMBlockMiniNode*> MiniNodes;

	FBIMPresetInstance SavePendingPreset;
	FBIMEditorNodeIDType SavePendingInstanceID;

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
	class AEditModelPlayerController *Controller;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UScaleBox *ScaleBoxForNodes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCanvasPanel *CanvasPanelForNodes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMEditColorPicker* BIM_EditColorBP;

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

	FBIMEditorNodeIDType SelectedNodeID;
	FBIMPresetEditor InstancePool;
	FBIMAssemblySpec CraftingAssembly;

	// Typedef'd as FBIMEditorNodeIDType...UPROPERTIES don't support typedef
	UPROPERTY()
	TMap<FName, class UBIMBlockNode*> IdToNodeMap;

	bool UpdateCraftingAssembly();
	void ToggleCollapseExpandNodes();
	void SetNodeAsSelected(const FBIMEditorNodeIDType& InstanceID);
	float GetCurrentZoomScale() const;
	bool EditPresetInBIMDesigner(const FGuid& PresetID);
	bool SetPresetForNodeInBIMDesigner(const FBIMEditorNodeIDType& InstanceID, const FGuid& PresetID);
	void UpdateBIMDesigner(bool AutoAdjustToRootNode = false);
	void AutoArrangeNodes();
	void DrawConnectSplineForNodes(const FPaintContext& context, class UBIMBlockNode* StartNode, class UBIMBlockNode* EndNode) const;
	void DrawConnectSplineForMiniNode(const FPaintContext& context, class UBIMBlockNode* StartNode, class UBIMBlockMiniNode* MiniNode) const;
	FGuid GetPresetID(const FBIMEditorNodeIDType& InstanceID);
	bool DeleteNode(const FBIMEditorNodeIDType& InstanceID);
	bool AddNodeFromPreset(const FBIMEditorNodeIDType& ParentID, const FGuid& PresetID, int32 ParentSetIndex);
	bool ApplyBIMFormElement(const FBIMEditorNodeIDType& NodeID, const FBIMPresetFormElement& FormElement);
	bool GetNodeForReorder(const FVector2D &OriginalNodeCanvasPosition, const FBIMEditorNodeIDType& NodeID);
	bool SavePresetFromNode(bool SaveAs, const FBIMEditorNodeIDType& InstanceID);
	bool ConfirmSavePendingPreset();
	void ToggleSlotNode(const FBIMEditorNodeIDType& ParentID, int32 SlotID, bool NewEnable);
	void CheckMouseButtonDownOnBIMDesigner();
	void ToggleColorPickerVisibility(bool NewVisibility);
};
