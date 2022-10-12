// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Objects/ModumateObjectEnums.h"
#include "EditModelUserWidget.generated.h"

/**
 *
 */
UENUM(BlueprintType)
enum class ELeftMenuState : uint8
{
	ViewMenu,
	CutPlaneMenu,
	ToolMenu,
	SelectMenu,
	BrowserMenu,
	SwapMenu,
	DeleteMenu,
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

	UFUNCTION()
	void UpdateOnToolModeChanged();

	UFUNCTION()
	void UpdateToolTray();

public:

	UPROPERTY()
	class AEditModelPlayerController *Controller;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UProjectSystemWidget* ProjectSystemMenu;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolbarWidget *ToolbarWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayWidget *ToolTrayWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USelectionTrayWidget *SelectionTrayWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMDesigner *BIMDesigner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMDebugger* BIMDebugger;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUserWidget* DatasmithDebugger;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UDrawingDesignerWebBrowserWidget* DrawingDesigner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBIMBlockDialogBox *BIMBlockDialogBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UViewMenuWidget *ViewMenu;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UCutPlaneMenuWidget *CutPlaneMenu;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModalDialogWidget* ModalDialogWidgetBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UTutorialWalkthroughMenu* TutorialWalkthroughMenuBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHelpMenu* HelpMenuBP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UViewCubeWidget* ViewCubeUserWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBrowserMenuWidget* BrowserMenuWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USwapMenuWidget* SwapMenuWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UDeleteMenuWidget* DeleteMenuWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateSettingsMenu* SettingsMenuWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateChatWidget* TextChatWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListVerticalWidget* UsersListVertical;
	
	UPROPERTY()
	TMap<EToolMode, class UModumateButtonUserWidget*> ToolToButtonMap;

	UPROPERTY()
	class UModumateButtonUserWidget* CurrentActiveToolButton;

	ELeftMenuState PreviousLeftMenuState = ELeftMenuState::None;
	ELeftMenuState CurrentLeftMenuState = ELeftMenuState::None;

	bool bIsHelpMenuVisible = false;
	bool bIsShowDrawingDesigner = false;

	UFUNCTION()
	void EMOnSelectionObjectChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void EventToggleProjectMenu();

	void EditExistingAssembly(const FGuid& AssemblyGUID);
	void ToggleBIMDesigner(bool Open);
	void SwitchLeftMenu(ELeftMenuState NewMenuState, EToolCategories OptionalToolCategory = EToolCategories::Unknown);
	void UpdateCutPlanesList();
	bool RemoveCutPlaneFromList(int32 ObjID = MOD_ID_NONE);
	bool UpdateCutPlaneInList(int32 ObjID = MOD_ID_NONE);
	void RefreshAssemblyList(bool bScrollToSelected = false);
	void UpdateViewModeIndicator(EEditViewModes NewViewMode);
	void ShowBIMDebugger(bool bNewVisible);
	void ToggleDatasmithDebugger();
	bool IsBIMDebuggerOn();
	void ToggleCutPlaneMenu(bool bNewVisibility);
	void ToggleViewMenu(bool bNewVisibility);
	void ToggleBrowserMenu(bool bNewVisibility);
	void ToggleSwapMenu(bool bNewVisibility);
	void ToggleDeleteMenu(bool bNewVisibility);
	void ToggleSettingsWindow(bool bNewVisibility);
	void ToggleHelpMenu(bool bNewVisibility);
	void ToggleTextChat(bool bNewVisibility);
	void ToggleVerticalUserList(bool bNewVisibility);
	void ToggleViewOnlyBadge(bool bNewVisibility);
	bool IsBIMDesingerActive() const;
	bool EMUserWidgetHandleEscapeKey();
	void UpdateSelectTrayVisibility();
	FText GetPlanUpgradeRichText();
	void UpdateMoveRotateToolButtonsUsability();
	void UpdateUsersList();
	void EditDetailDesignerFromSelection();
	void UpdateViewCubeOffset(float InOffset);
	void CheckDeletePresetFromWebUI(const FGuid& PresetGUIDToDelete);
};
