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

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UNCPNavigator* NCPNavigatorWidget;

	void BuildBrowserMenu();
};
