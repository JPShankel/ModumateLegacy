// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "UsersListHorizontalWidget.generated.h"

/**
 *
 */

class AEditModelPlayerState;

UCLASS()
class MODUMATE_API UUsersListHorizontalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUsersListHorizontalWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox* HorizontalBox_UsersList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* TextBlock_NumberOfUsers;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButton_ExpandUsersList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UUsersListButtonWidget> UsersListButtonWidgetClass;

	UFUNCTION()
	void OnReleaseButton_ExpandUsersList();

	void UpdateHorizontalUsersList(const TArray<AEditModelPlayerState*>& InPlayerStates, int32 ExtraNumberOfPlayerStates);
};
