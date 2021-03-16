// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMTagPath.h"
#include "NCPButton.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UNCPButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UNCPButton(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	UPROPERTY()
	class UNCPNavigator* ParentNCPNavigator;

	FBIMTagPath NCPTag;
	int32 TagOrder;
	bool bExpanded = false;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateTextBlock* ButtonText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText ButtonTextOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSlateColor SelectedColor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSlateColor NonSelectedColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ButtonPaddingSizePerNCPorder = 12.f;

	UFUNCTION()
	void OnButtonPress();

	void BuildButton(class UNCPNavigator* InParentNCPNavigator, const FBIMTagPath& InNCP, int32 InTagOrder, bool bBuildAsExpanded = false);
	void ToggleTextColor(bool bAsSelected);
};
