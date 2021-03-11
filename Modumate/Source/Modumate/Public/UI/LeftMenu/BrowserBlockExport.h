// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BrowserBlockExport.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBrowserBlockExport : public UUserWidget
{
	GENERATED_BODY()

public:
	UBrowserBlockExport(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonExport;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonCancel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* ExportRemainText;

	UPROPERTY()
	class UBrowserMenuWidget* ParentBrowserMenuWidget;

	UFUNCTION()
	void OnButtonExportReleased();

	UFUNCTION()
	void OnButtonCancelReleased();
};
