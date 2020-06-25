// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EditModelUserWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UEditModelUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UEditModelUserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolbarWidget *ToolbarWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayWidget *ToolTrayWidget;

	UFUNCTION()
	void EMOnToolModeChanged();
};
