// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnrealClasses/TooltipManager.h"

#include "TooltipWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTooltipWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateTextBlockUserWidget* TextBlock_Title;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateTextBlockUserWidget* TextBlock_Desc;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UHorizontalBox* HorizontalBoxForKeys;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetAnim))
	class UWidgetAnimation* FadeInAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Menu)
	TSubclassOf<class UTooltipKeyTextWidget> TooltipKeyTextWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Menu)
	TSubclassOf<class UTooltipKeyTextWidget> NonBoxedTooltipKeyTextWidgetClass;

	void BuildPrimaryTooltip(const FTooltipData& InData);
	void BuildSecondaryTooltip(const FTooltipData& InData);
	void AddWidgetToHorizontalBoxForKeys(class UWidget* InWidget);
};
