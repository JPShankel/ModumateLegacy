// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMEditColorMap.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMEditColorMap : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMEditColorMap(const FObjectInitializer& ObjectInitializer);
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
	FName ColorGradientMapHueParamName = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D ColorGradientMapSize = FVector2D(138.f, 158.f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* ColorGradientMap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* PickerIcon;

	void PerformDrag();
	void BuildColorMap(const FLinearColor& InHSV, class UBIMEditColorPicker* InParentColorPicker);
	void UpdateColorMapGradient(float InHue);
};
