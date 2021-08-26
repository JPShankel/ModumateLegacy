// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMEditColorBar.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMEditColorBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMEditColorBar(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	class UBIMEditColorPicker* ParentColorPicker;

	bool DragTick = false;
	int32 MouseClickCountdown=0;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ColorBarSize = 158.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* ColorLevelBarLeft;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* ColorLevelBarRight;

	void PerformDrag();
	void BuildColorBar(const FLinearColor& InHSV, class UBIMEditColorPicker* InParentColorPicker);
};
