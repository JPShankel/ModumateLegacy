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
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY()
	class UEditModelUserWidget *EditModelUserWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_Metaplanes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_Separators;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_SurfaceGraphs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_Attachments;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_3DViews;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *Button_CutPlanes;

	UFUNCTION()
	void OnButtonReleaseMetaPlane();

	UFUNCTION()
	void OnButtonReleaseSeparators();

	UFUNCTION()
	void OnButtonReleaseSurfaceGraphs();

	UFUNCTION()
	void OnButtonReleaseAttachments();

	UFUNCTION()
	void OnButtonRelease3DViews();

	UFUNCTION()
	void OnButtonReleaseCutPlanes();

protected:

};
