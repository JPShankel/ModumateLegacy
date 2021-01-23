// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ViewMenuBlockSavedViews.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UViewMenuBlockSavedViews : public UUserWidget
{
	GENERATED_BODY()

public:
	UViewMenuBlockSavedViews(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView *SavedViewsList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonAdd;

	UFUNCTION()
	void OnButtonAddReleased();

	void UpdateSavedViewsList();
};
