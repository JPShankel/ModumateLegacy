// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

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

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USelectionTrayBlockPresetList *SelectionTrayBlockPresetList;

	void OpenToolTrayForSelection();
	void OpenToolTrayForSwap();
	void CloseToolTray();

protected:

};
