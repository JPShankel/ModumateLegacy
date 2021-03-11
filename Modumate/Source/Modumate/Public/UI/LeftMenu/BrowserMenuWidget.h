// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BrowserMenuWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBrowserMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBrowserMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UNCPNavigator* NCPNavigatorWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBrowserBlockSettings* BrowserBlockSettingsWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBrowserBlockExport* BrowserBlockExportWidget;

	void BuildBrowserMenu();
	void ToggleExportWidgetMenu(bool bEnable);
	void AskToExportEstimates();
};
