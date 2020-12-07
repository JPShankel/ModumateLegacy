// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "BIMKernel/Core/BIMKey.h"

#include "BIMDebugPresetListItem.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UBIMDebugPresetListItem : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UBIMDebugPresetListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	FBIMKey PresetKey;
	bool bIsFromHistoryMenu = false;
	class UBIMDebugger* ParentDebugger;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidgetOptional))
	class UModumateButton* ModumateButtonMain;

	UFUNCTION()
	void OnButtonMainReleased();

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
