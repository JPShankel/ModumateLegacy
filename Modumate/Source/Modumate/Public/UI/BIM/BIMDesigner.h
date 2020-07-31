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

	bool DragTick = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	bool DragReset = true;

	UPROPERTY()
	TArray<class UBIMBlockNode*> BIMBlockNodes;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey DragKey = EKeys::MiddleMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomDeltaPlus = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomDeltaMinus = 0.91f;

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

	float GetCurrentZoomScale();
};
