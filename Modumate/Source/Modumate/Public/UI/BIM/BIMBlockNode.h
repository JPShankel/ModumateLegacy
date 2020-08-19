// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMNodeEditor.h"

#include "BIMBlockNode.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class ENodeWidgetSwitchState : uint8
{
	Collapsed,
	Expanded,
	PendingSwap,
	None
};


UCLASS()
class MODUMATE_API UBIMBlockNode : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMBlockNode(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY()
	class UBIMDesigner *ParentBIMDesigner;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	bool DragTick = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	bool DragReset = true;
	bool NodeCollapse = true;
	bool NodeDirty = false;
	ENodeWidgetSwitchState NodeSwitchState = ENodeWidgetSwitchState::None;

public:
	// Size of node, should be constant regardless of dirty/collapse state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float NodeWidth = 312.f;

	// Size of the tab on top of the node during dirty state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float DirtyTabSize = 80.f;

	// Size of the node during collapse state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float CollapsedNodeSize = 120.f;

	// Size of the node during swap state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float SwapNodeSize = 500.f;

	// Size from bottom edge the dirty tab to the top of the first form item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float ExpandedImageSize = 310.f;

	// Size of form items, such as drop down and user input field
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float FormItemSize = 30.f;

	// Size from bottom edge of the last form item to the bottom of the node
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float BottomPadding = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey DragKey = EKeys::LeftMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey ToggleCollapseKey = EKeys::LeftMouseButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *GrabHandleImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UComponentPresetListItem *ComponentPresetListItem;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSwapCollapsed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSwapExpanded;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockAssembliesList *SelectionTray_Block_Swap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUserWidget *BIMBlockNodeDirty;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder *DirtyStateBorder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWidgetSwitcher *NodeSwitcher;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UButton *Button_Debug;

	FName PresetID;
	int32 ID = -1;
	int32 ParentID = -1;
	bool IsKingNode = false;

	UFUNCTION()
	void PerformDrag();

	UFUNCTION()
	void OnButtonSwapReleased();

	void UpdateNodeDirty(bool NewDirty);
	void UpdateNodeCollapse(bool NewCollapse, bool AllowAutoArrange = false);
	bool BuildNode(class UBIMDesigner *OuterBIMDesigner, const FBIMCraftingTreeNodeSharedPtr &Node);
	void UpdateNodeSwitchState(ENodeWidgetSwitchState NewState);

	UFUNCTION(BlueprintPure)
	FVector2D GetEstimatedNodeSize();
};
