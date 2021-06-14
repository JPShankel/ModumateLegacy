// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "HelpBlockTutorialSearch.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UHelpBlockTutorialSearch : public UUserWidget
{
	GENERATED_BODY()

public:
	UHelpBlockTutorialSearch(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* ComponentSearchBar;

	UFUNCTION()
	void OnSearchBarChanged(const FText& NewText);
};
