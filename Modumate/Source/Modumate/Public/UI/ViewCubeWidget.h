// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ViewCubeWidget.generated.h"

UCLASS()
class MODUMATE_API UViewCubeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UViewCubeWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UFUNCTION()
	void OnButtonPressedSnapUp();

	UFUNCTION()
	void OnButtonPressedSnapDown();

	UFUNCTION()
	void OnButtonPressedSnapLeft();

	UFUNCTION()
	void OnButtonPressedSnapRight();

	UFUNCTION()
	void OnButtonPressedSnapForward();

	UFUNCTION()
	void OnButtonPressedSnapBackward();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSnapUp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSnapDown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSnapLeft;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSnapRight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSnapForward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSnapBackward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float LineThickness;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FColor LineColor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float CubeSideLength;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector DefaultCameraForward;

private:
	void Zoom(const FVector& InForward, const FVector& InUp);
	FVector2D TransformVector(const FVector& InVector) const;

private:
	UPROPERTY()
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	class APlayerCameraManager* CameraManager;

	UPROPERTY()
	TMap<FVector, class UModumateButtonUserWidget*> DirectionToButton;
};
