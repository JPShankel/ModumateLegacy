// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Objects/ModumateObjectEnums.h"

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

	EToolCategories CurrentToolCategory;

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
	class UNCPNavigator* NCPNavigatorAssembliesList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UGeneralListItemMenuBlock* ToolTrayBlockTerrainList;

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToMetaPlaneTools();

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToSeparatorTools(EToolMode Toolmode);

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToSurfaceGraphTools();

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToAttachmentTools(EToolMode Toolmode);

	UFUNCTION(BlueprintCallable)
	bool ChangeBlockToSiteTools();

	void HideAllToolTrayBlocks();
	bool IsToolTrayVisible();
	void OpenToolTray();
	void CloseToolTray();
};
