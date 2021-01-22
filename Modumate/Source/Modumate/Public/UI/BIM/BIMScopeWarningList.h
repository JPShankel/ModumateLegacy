// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMScopeWarningList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMScopeWarningList : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMScopeWarningList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	FString CurrentDefinitionName;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* TypeText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* PresetItemList;

	void AddItemToVerticalList(class UBIMScopeWarningListItem* ItemToAdd, const FMargin& ItemPadding);
	void BuildPresetListTitle(const FString& DefinitionName);
};
