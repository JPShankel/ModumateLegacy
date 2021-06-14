// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"

#include "ToolbarTopWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolbarTopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolbarTopWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY()
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	class UEditModelUserWidget* EditModelUserWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonModumateHome;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_ViewModeMetaGraph;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_ViewModeSeparator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_ViewModeSurfaceGraph;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_ViewModeAllObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_ViewModePhysical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* Button_Help;

	UFUNCTION()
	void OnButtonReleaseModumateHome();

	UFUNCTION()
	void OnButtonReleaseButtonHelp();

	void SwitchToViewMode(EEditViewModes NewViewMode);

};
