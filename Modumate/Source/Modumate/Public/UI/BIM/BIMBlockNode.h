// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMBlockNode.generated.h"

/**
 *
 */


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

	bool DragTick = false;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	bool DragReset = true;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey DragKey = EKeys::LeftMouseButton;

	UPROPERTY()
	class UBIMDesigner *BIMDesigner;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	// TODO: Limit enable drag to DragHandle
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	//class UImage *DragHandle;

	UFUNCTION()
	void PerformDrag();
};
