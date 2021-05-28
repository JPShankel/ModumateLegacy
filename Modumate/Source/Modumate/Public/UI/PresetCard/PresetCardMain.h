// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Database/ModumateObjectEnums.h"
#include "PresetCardMain.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class EPresetCardType : uint8
{
	Browser,
	SelectTray,
	Swap,
	AssembliesList,
	Delete,
	None
};

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

	UPROPERTY()
	class UNCPNavigator* ParentNCPNavigator;

	UPROPERTY()
	class USelectionTrayBlockPresetList* ParentSelectionTrayBlockPresetList;

	UPROPERTY()
	class UBrowserItemObj* ParentBrowserItemObj;

	UPROPERTY()
	class UPresetCardItemObject* ParentPresetCardItemObj;

	FGuid PresetGUID;
	bool bAllowInteraction = true;
	EPresetCardType CurrentPresetCardType = EPresetCardType::None;

	// For SelectTray type preset card
	bool bBuildAsObjectTypeSelect = false;
	EObjectType SelectedObjectType;
	int32 SelectCount = 0;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* MainButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* DynamicVerticalBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* MainVerticalBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* DropShadowImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMargin MainVerticalBoxSlotCollapsedPadding = FMargin(0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FMargin MainVerticalBoxSlotSelectedPadding = FMargin(0.f, 0.f, 0.f, 12.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor UserInteractionDisableBGColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardHeader> PresetCardHeaderSelectedClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardHeader> PresetCardHeaderCollapsedClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardPropertyList> PresetCardPropertyListClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardObjectList> PresetCardObjectListClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardQuantityList> PresetCardQuantityListClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardMetaDimension> PresetCardMetaDimensionClass;

	UFUNCTION()
	void OnMainButtonReleased();

	// Builder
	void BuildAsCollapsedPresetCard(const FGuid& InPresetKey, bool bInAllowInteraction);
	void BuildAsExpandedPresetCard(const FGuid& InPresetKey);
	void SetAsNCPNavigatorPresetCard(class UNCPNavigator* InParentNCPNavigator, class UBrowserItemObj* InBrowserItemObj, EPresetCardType InPresetCardType);

	void UpdateSelectionItemCount(int32 ItemCount);
	bool IsCurrentToolAssembly();
	void BuildAsAssemblyListPresetCard(class UBrowserItemObj* InBrowserItemObj);

	void ToggleMainButtonInteraction(bool bEnable);
	void ClearWidgetPool(class UPanelWidget* Widget);

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
