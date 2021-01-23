// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMTagPath.h"

#include "NCPSwitcherButton.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UNCPSwitcherButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UNCPSwitcherButton(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

	FBIMTagPath NCPTag;
public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateTextBlock* ButtonText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText ButtonTextOverride;

	UFUNCTION()
	void OnButtonPress();

	void BuildButton(const FBIMTagPath& InNCP);

};
