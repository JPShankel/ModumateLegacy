// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BrowserBlockSettings.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBrowserBlockSettings : public UUserWidget
{
	GENERATED_BODY()

public:
	UBrowserBlockSettings(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget* ButtonIconWithTextExportWidget;

	UPROPERTY()
	class UBrowserMenuWidget* ParentBrowserMenuWidget;

	UFUNCTION()
	void OnExportButtonReleased();

	void ToggleEnableExportButton(bool bEnable);
};
