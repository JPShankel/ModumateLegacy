// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "AdjustmentHandleWidget.generated.h"


UCLASS()
class MODUMATE_API UAdjustmentHandleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAdjustmentHandleWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	void SetMainButtonStyle(const class USlateWidgetStyleAsset *ButtonStyleAsset);
	void UpdateTransform();
	bool IsButtonHovered() const { return bIsButtonHovered; }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UButton *MainButton;

	UPROPERTY()
	class AAdjustmentHandleActor *HandleActor;

	UPROPERTY()
	FVector2D DesiredSize;

	UPROPERTY()
	FVector2D MainButtonOffset;

protected:
	bool bIsButtonHovered;

	virtual void OnWidgetRebuilt() override;

	void OnHoverChanged(bool bNewHovered);

	UFUNCTION()
	void OnPressed();

	UFUNCTION()
	void OnHovered();

	UFUNCTION()
	void OnUnhovered();
};
