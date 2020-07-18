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

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolbarWidget *ToolbarWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayWidget *ToolTrayWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USelectionTrayWidget *SelectionTrayWidget;

	UFUNCTION()
	void EMOnToolModeChanged();

	UFUNCTION()
	void EMOnSelectionObjectChanged();

	// Blueprint event for opening BIM configurator
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void EventNewCraftingAssembly(EToolMode ToolMode);

	// Blueprint event for opening BIM configurator for crafting existing assembly
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void EventEditExistingAssembly(EToolMode ToolMode, FName AssemblyKey);
};
