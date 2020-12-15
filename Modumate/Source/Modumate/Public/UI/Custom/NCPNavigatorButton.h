// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMTagPath.h"

#include "NCPNavigatorButton.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UNCPNavigatorButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UNCPNavigatorButton(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	FBIMTagPath ButtonNCP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateTextBlock* ButtonText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText ButtonTextOverride;

	UFUNCTION()
	void OnButtonPress();

	void BuildButton(const FBIMTagPath& InButtonNCP, const FString& ButtonDisplayName);
};
