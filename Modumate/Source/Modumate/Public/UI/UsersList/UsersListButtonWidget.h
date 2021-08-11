// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UsersListButtonWidget.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UUsersListButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUsersListButtonWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* TextBlock_UserName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButton;

	void SetupFromPlayerState(const class AEditModelPlayerState* InOwnerPlayerState);
};
