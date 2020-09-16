// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMKey.h"

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

	FBIMKey CurrentPresetToSwap;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USelectionTrayBlockPresetList *SelectionTrayBlockPresetList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockAssembliesList *SelectionTray_Block_Swap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWidgetSwitcher *WidgetSwitcherTray;

	void OpenToolTrayForSelection();
	void OpenToolTrayForSwap(EToolMode ToolMode, const FBIMKey& PresetToSwap);
	void CloseToolTray();
	const FBIMKey &GetCurrentPresetToSwap() const { return CurrentPresetToSwap; }

protected:

};
