// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ModumateTextBlockUserWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateTextBlockUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateTextBlockUserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName TooltipID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock *ModumateTextBlock;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EllipsizeWordAt = 18;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool AutoWrapTextOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float WrapTextAtOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETextWrappingPolicy WrappingPolicyOverride = ETextWrappingPolicy::DefaultWrapping;

	void ChangeText(const FText &NewText, bool EllipsizeText = true);

protected:

	UFUNCTION()
	UWidget* OnTooltipWidget();
};
