// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"

#include "BIMDebugger.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UBIMDebugger : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMDebugger(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	FBIMKey LastSelectedKey;

	virtual void NativeConstruct() override;


public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* PresetList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ButtonUpdatePresetList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ButtonClearHistory;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UMultiLineEditableText* TextDisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UMultiLineEditableText* TextGUID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UMultiLineEditableText* TextBIMKey;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UMultiLineEditableText* TextOtherInfo;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* DependentPresetsList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView* PresetHistoryList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UEditableText* SearchField;

	UFUNCTION()
	void OnButtonUpdatePresetListReleased();

	UFUNCTION()
	void ClearPresetHistory();

	UFUNCTION()
	void OnSearchFieldChanged(const FText& NewText);

	EBIMResult ConstructPresetList();
	EBIMResult DebugBIMPreset(const FBIMKey& PresetKey, bool AddToHistory = true);
	bool IsPresetAvailableForSearch(const FBIMPresetInstance& SearchPreset);
};