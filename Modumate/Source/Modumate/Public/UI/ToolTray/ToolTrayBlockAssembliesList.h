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
	FBIMEditorNodeIDType CurrentNodeForSwap;
	EToolMode SwapSelectionToolMode = EToolMode::VE_NONE;
	FGuid SwapSelectionPresetID;
	FBIMPresetFormElement FormElement;

public:
	UPROPERTY()
	class UToolTrayWidget *ToolTray;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonAdd;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UComponentPresetListItem* ComponentPresetItem;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UNCPNavigator* NCPNavigatorAssembliesList;

	void RefreshNCPNavigatorAssembliesList(bool bScrollToSelected = false);
	void ResetSearchBox();

	UFUNCTION()
	void OnButtonAddReleased();
};
