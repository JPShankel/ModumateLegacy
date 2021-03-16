// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "BrowserItemWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBrowserItemWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UBrowserItemWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UPresetCardMain* PresetCardMainWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UNCPButton* NCPButtonWidget;

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
