// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DrawingDesignerWebBrowserWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UDrawingDesignerWebBrowserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDrawingDesignerWebBrowserWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateWebBrowser* DrawingSetWebBrowser;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* DebugSubmit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UMultiLineEditableTextBox* DebugDocumentTextBox;

	UFUNCTION()
	void OnDebugSubmit();

	void InitWithController();
};
