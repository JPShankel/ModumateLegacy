// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NCPNavigator.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UNCPNavigator : public UUserWidget
{
	GENERATED_BODY()

public:
	UNCPNavigator(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* DynamicMainList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UNCPButton> NCPButtonClass;

	void BuildNCPNavigator();
};
