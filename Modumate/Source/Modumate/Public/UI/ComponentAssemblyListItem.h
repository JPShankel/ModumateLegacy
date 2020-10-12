// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMKernel/BIMKey.h"

#include "ComponentAssemblyListItem.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class EComponentListItemType : uint8
{
	AssemblyListItem,
	SelectionListItem,
	SwapListItem,
	SwapDesignerPreset,
	None
};

UCLASS()
class MODUMATE_API UComponentAssemblyListItem : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UComponentAssemblyListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	FBIMKey AsmKey; // Unique key such as assembly key or presetID
	FText ItemDisplayName; // Display name, user-facing
	EToolMode ToolMode = EToolMode::VE_NONE;
	EComponentListItemType ItemType = EComponentListItemType::None;
	int32 BIMInstanceID = INDEX_NONE;
	bool bIsNonAssemblyObjectSelectItem = false;

	UPROPERTY()
	class AEditModelPlayerController_CPP *EMPlayerController;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UComponentPresetListItem *ComponentPresetItem;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButtonMain;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonEdit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonSwap;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonTrash;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonConfirm;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox *VerticalBoxProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateTextBlockUserWidget> ModumateTextBlockUserWidgetClass;

	UFUNCTION()
	void OnModumateButtonMainReleased();

	UFUNCTION()
	void OnButtonEditReleased();

	UFUNCTION()
	void OnButtonSwapReleased();

	UFUNCTION()
	void OnButtonConfirmReleased();

	void UpdateItemType(EComponentListItemType NewItemType);
	void UpdateSelectionItemCount(int32 ItemCount);
	bool BuildFromAssembly();
	bool GetItemTips(TArray<FString> &OutTips);

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
