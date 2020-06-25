// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ToolTrayWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolTrayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY()
	class UEditModelUserWidget *EditModelUserWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock *ToolTrayMainTitleBlock;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockTools *ToolTrayBlockTools;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockModes *ToolTrayBlockModes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockProperties *ToolTrayBlockProperties;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UToolTrayBlockAssembliesList *ToolTrayBlockAssembliesList;

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToMetaPlaneTools();

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToSeparatorTools();

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToSurfaceGraphTools();

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToAttachmentTools();

	void HideAllToolTrayBlocks();

protected:

};
