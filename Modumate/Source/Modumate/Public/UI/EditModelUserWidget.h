// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMKey.h"
#include "ModumateCore/ModumateTypes.h"
#include "Database/ModumateObjectEnums.h"
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuWidget *CutPlaneMenu;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UAlertAccountDialogWidget* AlertFreeAccountDialogWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UAlertAccountDialogWidget* AlertPausedAccountDialogWidget;

	ERightMenuState CurrentRightMenuState = ERightMenuState::None;

	UFUNCTION()
	void EMOnToolModeChanged();

	UFUNCTION()
	void EMOnSelectionObjectChanged();

	// Blueprint event for opening BIM configurator
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void EventNewCraftingAssembly(EToolMode ToolMode);

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void EventToggleProjectMenu();

	void EditExistingAssembly(EToolMode ToolMode, const FBIMKey& AssemblyKey);
	void ToggleBIMDesigner(bool Open);
	void SwitchRightMenu(ERightMenuState NewMenuState);
	void UpdateCutPlanesList();
	bool RemoveCutPlaneFromList(int32 ObjID = MOD_ID_NONE);
	bool UpdateCutPlaneVisibilityinList(bool IsVisible, int32 ObjID = MOD_ID_NONE);
	void RefreshAssemblyList();
	void ShowAlertPausedAccountDialog();
	void ShowAlertFreeAccountDialog();
	void UpdateViewModeIndicator(EEditViewModes NewViewMode);
};
