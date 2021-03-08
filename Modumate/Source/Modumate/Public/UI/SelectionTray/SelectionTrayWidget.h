// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/Core/BIMKey.h"

#include "SelectionTrayWidget.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API USelectionTrayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USelectionTrayWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	FGuid CurrentPresetToSwap;

	FGuid CurrentDetailPreset;
	TSet<int32> CurrentDetailEdgeIDs;

	bool GetDetailFromSelection(FGuid& OutDetailPreset, TSet<int32>& OutEdgeIDs);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USelectionTrayBlockPresetList *SelectionTrayBlockPresetList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockAssembliesList *SelectionTray_Block_Swap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWidgetSwitcher *WidgetSwitcherTray;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockProperties *ToolTrayBlockProperties;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UDetailDesignerContainer* DetailDesigner;

	void OpenToolTrayForSelection();
	void OpenToolTrayForSwap(EToolMode ToolMode, const FGuid& PresetToSwap);
	void OpenToolTrayDetailDesigner(const FGuid& DetailPreset, const TSet<int32>& EdgeIDs);
	void UpdateFromSelection();
	void StartDetailDesignerFromSelection();
	void CloseToolTray();
	const FGuid& GetCurrentPresetToSwap() const { return CurrentPresetToSwap; }
};
