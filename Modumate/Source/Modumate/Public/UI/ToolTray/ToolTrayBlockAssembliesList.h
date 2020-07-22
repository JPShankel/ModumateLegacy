// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToolTrayBlockAssembliesList.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolTrayBlockAssembliesList : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockAssembliesList(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY()
	class UToolTrayWidget *ToolTray;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UListView *AssembliesList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonAdd;

	UFUNCTION(BlueprintCallable)
	void CreateAssembliesListForCurrentToolMode();

	UFUNCTION()
	void OnButtonAddReleased();
};
