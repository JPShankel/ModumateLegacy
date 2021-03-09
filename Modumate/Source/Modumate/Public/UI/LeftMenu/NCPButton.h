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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* DynamicNCPList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UNCPButton> NCPButtonClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardMain> PresetCardMainClass;

	UFUNCTION()
	void OnButtonPress();

	void EmptyLists();
	void BuildButton(const FBIMTagPath& InNCP, int32 InTagOrder, bool bBuildAsExpanded = false);
	void BuildSubButtons();
	void ToggleTextColor(bool bAsSelected);
};
