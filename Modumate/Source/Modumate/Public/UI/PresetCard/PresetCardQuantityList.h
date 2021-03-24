// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PresetCardQuantityList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardQuantityList : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardQuantityList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	FGuid PresetGUID;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* DynamicQuantityList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* MainButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardQuantityListTotal> PresetCardQuantityListTotalClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardQuantityListSubTotal> PresetCardQuantityListSubTotalClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardQuantityListSubTotalListItem> PresetCardQuantityListSubTotalListItemClass;

	UFUNCTION()
	void OnMainButtonReleased();

	// Builder
	void BuildAsQuantityList(const FGuid& InGUID, bool bAsExpandedList);

	void EmptyList();
};
