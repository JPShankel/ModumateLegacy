// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Core/BIMProperties.h"
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
	class AEditModelPlayerController_CPP* Controller;

	UPROPERTY()
	class AEditModelGameState_CPP *GameState;

	ESwapType SwapType = ESwapType::None;
	FBIMKey NodeParentPresetID;
	FBIMKey NodePresetIDToSwap;
	int32 CurrentNodeForSwap = INDEX_NONE;
	EToolMode SwapSelectionToolMode = EToolMode::VE_NONE;
	FBIMKey SwapSelectionPresetID;
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

	UFUNCTION(BlueprintCallable)
	void CreateAssembliesListForCurrentToolMode();

	void CreatePresetListInNodeForSwap(const FBIMKey& ParentPresetID, const FBIMKey& PresetIDToSwap, int32 NodeID, const EBIMValueScope& InScope, const FBIMNameType& InNameType);
	void CreatePresetListInAssembliesListForSwap(EToolMode ToolMode, const FBIMKey& PresetID);
	bool IsPresetAvailableForSearch(const FBIMKey& PresetKey);
	void ResetSearchBox();

	UFUNCTION()
	void OnButtonAddReleased();

	UFUNCTION()
	void OnButtonCancelReleased();

	UFUNCTION()
	void OnSearchBarChanged(const FText& NewText);
};
