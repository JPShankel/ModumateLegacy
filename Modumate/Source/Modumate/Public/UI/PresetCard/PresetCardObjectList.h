// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PresetCardObjectList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UPresetCardObjectList : public UUserWidget
{
	GENERATED_BODY()

public:
	UPresetCardObjectList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	bool bIsDescendentlist;
	FGuid PresetGUID;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* MainButton;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* DynamicObjectList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* MainText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UPresetCardMain> PresetCardMainClass;

	UFUNCTION()
	void OnMainButtonReleased();

	// Builder
	void BuildAsDescendentList(const FGuid& InGUID, bool bAsExpandedList);
	void BuildAsAncestorList(const FGuid& InGUID, bool bAsExpandedList);

	void EmptyList();
};
