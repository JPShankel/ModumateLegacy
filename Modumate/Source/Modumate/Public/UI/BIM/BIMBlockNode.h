// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"

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
	FVector2D PreDragCanvasPosition = FVector2D::ZeroVector;

public:
	// Size of node, should be constant regardless of dirty/collapse state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float NodeWidth = 312.f;

	// Similar to NodeWidth, but for nodes with slots (parts)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float SlotNodeWidth = 442.f;

	// Size of the tab on top of the node during dirty state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EstimateSize")
	float DirtyTabSize = 100.f;

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

	// Opacity of node if it's not highlighted
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node Material")
	float NodeNonHighlightOpacity = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node Material")
	FName NodeAlphaParamName = TEXT("MaskAlpha");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey DragKey = EKeys::LeftMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey ToggleCollapseKey = EKeys::LeftMouseButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* GrabHandleImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UComponentPresetListItem* ComponentPresetListItem;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonSwapCollapsed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonSwapExpanded;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDeleteExpanded;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDeleteCollapsed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UVerticalBox* VerticalBoxProperties;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMBlockNodeDirtyTab* BIMBlockNodeDirty;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder* DirtyStateBorder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWidgetSwitcher* NodeSwitcher;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UButton* Button_Debug;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TitleNodeCollapsed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TitleNodeExpanded;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Preset_Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockUserEnterable> BIMBlockUserEnterableClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockDropdownPreset> BIMBlockDropdownPresetClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UBIMBlockSlotList* BIMBlockSlotList;

	UPROPERTY()
	class UMaterialInterface* IconMaterial;

	UPROPERTY()
	class UTexture* IconTexture;

	FBIMKey PresetID;
	int32 ID = INDEX_NONE;
	int32 ParentID = INDEX_NONE;
	bool IsKingNode = false;
	bool NodeDirty = false;
	bool NodeCollapse = true;
	bool bNodeHasSlotPart = false;
	bool bNodeHighlight = false;
	bool bNodeIsHidden = false;
	ENodeWidgetSwitchState NodeSwitchState = ENodeWidgetSwitchState::None;

	UFUNCTION()
	void PerformDrag();

	UFUNCTION()
	void OnButtonSwapReleased();

	UFUNCTION()
	void OnButtonDeleteReleased();

	UFUNCTION()
	void OnButtonDirtySave();

	UFUNCTION()
	void OnButtonDirtyAddNew();

	UFUNCTION()
	void OnButtonDirtyCancel();

	void UpdateNodeDirty(bool NewDirty);
	void UpdateNodeCollapse(bool NewCollapse);
	void UpdateNodeHidden(bool NewHide);
	bool BuildNode(class UBIMDesigner *OuterBIMDesigner, const FBIMPresetEditorNodeSharedPtr &Node, bool bAsSlot);
	void ReleaseNode();
	void UpdateNodeSwitchState(ENodeWidgetSwitchState NewState);
	void BeginDrag();
	void SetNodeAsHighlighted(bool NewHighlight);

	UFUNCTION(BlueprintPure)
	FVector2D GetEstimatedNodeSize();
};
