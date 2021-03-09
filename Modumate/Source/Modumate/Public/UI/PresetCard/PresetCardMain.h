// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "PresetCardMain.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardMain : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UPresetCardMain(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	FGuid PresetGUID;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* MainButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* DynamicVerticalBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder* MainBorder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* MainVerticalBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardHeader> PresetCardHeaderSelectedClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardHeader> PresetCardHeaderCollapsedClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardPropertyList> PresetCardPropertyListClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardObjectList> PresetCardObjectListClass;

	UFUNCTION()
	void OnMainButtonReleased();

	// Builder
	void BuildAsBrowserCollapsedPresetCard(const FGuid& InPresetKey);
	void BuildAsBrowserSelectedPresetCard(const FGuid& InPresetKey);

	void ClearWidgetPool(class UPanelWidget* Widget);

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
