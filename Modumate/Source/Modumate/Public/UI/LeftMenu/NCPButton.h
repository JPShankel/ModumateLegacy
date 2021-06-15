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

	bool bAsTutorialArticle = false;
	FGuid TutorialGUID;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateTextBlock* ButtonText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UHorizontalBox* HorizontalBoxText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UImage* ButtonIconGoToArticle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText ButtonTextOverride;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSlateColor SelectedColor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSlateColor NonSelectedColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ButtonPaddingSizePerNCPorder = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMargin ButtonPaddingSizeOriginal = FMargin(24.f, 3.f, 0.f, 3.f);

	UFUNCTION()
	void OnButtonPress();

	void BuildCategoryButton(class UNCPNavigator* InParentNCPNavigator, const FBIMTagPath& InNCP, int32 InTagOrder, bool bBuildAsExpanded = false);
	void BuildTutorialArticleButton(class UNCPNavigator* InParentNCPNavigator, const FGuid& InGUID, int32 InTagOrder);
	void ToggleTextColor(bool bAsSelected);
};
