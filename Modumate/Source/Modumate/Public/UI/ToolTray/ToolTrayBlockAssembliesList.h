// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "ToolTrayBlockAssembliesList.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class ESwapType : uint8
{
	SwapFromNode,
	SwapFromAssemblyList,
	SwapFromSelection,
	None
};

UCLASS()
class MODUMATE_API UToolTrayBlockAssembliesList : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockAssembliesList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	class AEditModelGameState *GameState;

	TArray<FGuid> AvailableBIMDesignerPresets;

	ESwapType SwapType = ESwapType::None;
	FBIMEditorNodeIDType CurrentNodeForSwap;
	EToolMode SwapSelectionToolMode = EToolMode::VE_NONE;
	FGuid SwapSelectionPresetID;
	EBIMValueScope SwapScope = EBIMValueScope::None;
	FBIMNameType SwapNameType = NAME_None;

public:
	UPROPERTY()
	class UToolTrayWidget *ToolTray;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView *AssembliesList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonAdd;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateButtonUserWidget *ButtonCancel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UComponentPresetListItem* ComponentPresetItem;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* Text_SearchBar;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UBIMBlockNCPNavigator* NCPNavigator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UBIMBlockNCPSwitcher* NCPSwitcher;

	UFUNCTION(BlueprintCallable)
	void CreateAssembliesListForCurrentToolMode();

	void CreatePresetListInNodeForSwap(const FGuid& ParentPresetID, const FGuid& PresetIDToSwap, const FBIMEditorNodeIDType& NodeID, const EBIMValueScope& InScope, const FBIMNameType& InNameType);
	void CreatePresetListForSwapFronNCP(const FBIMTagPath& InNCP);
	void AddBIMDesignerPresetsToList();
	void CreatePresetListInAssembliesListForSwap(EToolMode ToolMode, const FGuid& PresetID);
	bool IsPresetAvailableForSearch(const FGuid& PresetKey);
	void ResetSearchBox();

	UFUNCTION()
	void OnButtonAddReleased();

	UFUNCTION()
	void OnButtonCancelReleased();

	UFUNCTION()
	void OnSearchBarChanged(const FText& NewText);
};
