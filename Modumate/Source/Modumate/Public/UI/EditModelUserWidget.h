// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EditModelUserWidget.generated.h"

/**
 *
 */
UENUM(BlueprintType)
enum class ERightMenuState : uint8
{
	ViewMenu,
	CutPlaneMenu,
	None
};

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMDesigner *BIMDesigner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMBlockDialogBox *BIMBlockDialogBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UViewMenuWidget *ViewMenu;

	ERightMenuState CurrentRightMenuState = ERightMenuState::None;

	UFUNCTION()
	void EMOnToolModeChanged();

	UFUNCTION()
	void EMOnSelectionObjectChanged();

	// Blueprint event for opening BIM configurator
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void EventNewCraftingAssembly(EToolMode ToolMode);

	void EditExistingAssembly(EToolMode ToolMode, FName AssemblyKey);
	void ToggleBIMDesigner(bool Open);
	void SwitchRightMenu(ERightMenuState NewMenuState);

};
