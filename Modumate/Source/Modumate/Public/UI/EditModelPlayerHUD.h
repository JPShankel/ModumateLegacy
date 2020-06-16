// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ModumateCore/ModumateTypes.h"
#include "EditModelPlayerHUD.generated.h"

class UImage;
class URoofPerimeterPropertiesWidget;
class UDimensionWidget;
class AArcActor;

USTRUCT(BlueprintType)
struct FAffordanceLine
{
	GENERATED_USTRUCT_BODY();

public:

	FAffordanceLine() {}

	FAffordanceLine(const FVector &InStartPoint, const FVector &InEndPoint,
		const FLinearColor &InColor = FLinearColor::White, float InInterval = 0.0f,
		float InThickness = 2.0f, int32 InDrawPriority = 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector StartPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FVector EndPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float Interval = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	float Thickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	int32 DrawPriority = 0;
};

/**
 *
 */
UCLASS()
class MODUMATE_API AEditModelPlayerHUD : public AHUD
{
	GENERATED_UCLASS_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<class ALineActor*> All3DLineActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tools")
	bool RequestStaticCameraView = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<URoofPerimeterPropertiesWidget> RoofPerimeterPropertiesClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widgets")
	TSubclassOf<UDimensionWidget> DimensionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors")
	TSubclassOf<AArcActor> ArcClass;

	// Take a viewport screenshot, saves it in the playerController, then applies it to an UImage.
	// Should only be used during viewport draw, else there will be a TextureRHI ensure
	UFUNCTION(BlueprintCallable, Category = HUD)
	bool StaticCameraViewScreenshot(const FVector2D &ViewportSize, AEditModelPlayerController_CPP *EMPlayerController, UImage *ImageUI);
};
