// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

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
	TArray<class UBIMBlockNode*> BIMBlockNodes;

	TMap<int32, class UBIMBlockNode*> IdToNodeMap;
	TMap<FIntPoint, class UBIMBlockNode*> NodeCoordinateMap;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey DragKey = EKeys::MiddleMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomDeltaPlus = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomDeltaMinus = 0.91f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor NodeSplineColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeSplineThickness= 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeSplineBezierStartPercentage = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeSplineBezierEndPercentage = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeHorizontalSpacing = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NodeVerticalSpacing = 20.f;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UScaleBox *ScaleBoxForNodes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCanvasPanel *CanvasPanelForNodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMBlockNode> BIMBlockNodeClass;

	UFUNCTION()
	void PerformDrag();

	float GetCurrentZoomScale() const;
	bool EditPresetInBIMDesigner(const FName& PresetID);
	void AutoArrangeNodes();
	void DrawConnectSplineForNodes(const FPaintContext& context, class UBIMBlockNode* StartNode, class UBIMBlockNode* EndNode) const;
};
