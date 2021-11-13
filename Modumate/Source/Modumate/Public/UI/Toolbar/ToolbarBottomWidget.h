// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ToolbarBottomWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolbarBottomWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolbarBottomWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonMainModel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDrawingDesigner;

	UFUNCTION()
	void OnButtonReleaseMainModel();

	UFUNCTION()
	void OnButtonReleaseDrawingDesigner();

};
