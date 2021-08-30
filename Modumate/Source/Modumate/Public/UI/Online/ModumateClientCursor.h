// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ModumateClientCursor.generated.h"


UCLASS()
class MODUMATE_API UModumateClientCursor : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* IconImage = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	UMaterialInterface* CursorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float OcclusionThreshold = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float OccludedColorAlpha = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName MaterialClientColorParam;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName MaterialBackgroundColorParam;

	void InitCursor(const FLinearColor& InClientColor, const FLinearColor& InBackgroundColor);
	bool UpdateCursorOcclusion(const FVector& WorldLocation);

protected:

	UPROPERTY()
	class AEditModelPlayerController* EditModelPlayerController = nullptr;

	UPROPERTY()
	class UMaterialInstanceDynamic* DynamicCursorMaterial = nullptr;
	
	FLinearColor ClientColor;
	FLinearColor BackgroundColor;
};
