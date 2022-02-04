// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Objects/ModumateObjectEnums.h"
#include "UnrealClasses/EditModelInputHandler.h"

#include "ViewModeIndicatorWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UViewModeIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UViewModeIndicatorWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* ViewModeText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* ViewModeImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconMetaPlanes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconSeparators;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconSurfaceGraphs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconAllObjects;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconPhysical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* IndicatorButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextMetaPlanes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextSeparators;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextSurfaceGraphs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextAllObjects;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextPhysical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EInputCommand InputCommand;

	UFUNCTION()
	void OnIndicatorButtonPress();

	void SwitchToViewMode(EEditViewModes NewViewMode);

protected:

	UFUNCTION()
	UWidget* OnTooltipWidget();

};
