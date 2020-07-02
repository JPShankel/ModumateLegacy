// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ToolbarWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolbarWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY()
	class UEditModelUserWidget *EditModelUserWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_Metaplanes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_Separators;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_Attachments;

	UFUNCTION()
	void OnButtonPressMetaPlane();

	UFUNCTION()
	void OnButtonPressSeparators();

	UFUNCTION()
	void OnButtonPressSurfaceGraphs();

	UFUNCTION()
	void OnButtonPressAttachments();

protected:

};
