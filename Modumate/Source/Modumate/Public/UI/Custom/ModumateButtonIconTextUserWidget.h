// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnrealClasses/EditModelInputHandler.h"

#include "ModumateButtonIconTextUserWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateButtonIconTextUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateButtonIconTextUserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *ButtonText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *ButtonIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D *ButtonImageOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText ButtonTextOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ButtonTextEllipsizeWordAt = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool ButtonTextAutoWrapTextOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ButtonTextWrapTextAtOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETextWrappingPolicy ButtonTextWrappingPolicyOverride = ETextWrappingPolicy::DefaultWrapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TooltipID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EInputCommand InputCommand;

	UFUNCTION()
	void OnButtonPress();

protected:

	UFUNCTION()
	UWidget* OnTooltipWidget();
};
