// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/UsersList/UsersListButtonWidget.h"
#include "UI/Custom/TooltipWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UsersListHorizontalWidget.generated.h"

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
	class UModumateTextBlock* TextBlock_NumberOfUsers;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton* ModumateButton_ExpandUsersList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UsersListRoundButtonWidget_0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UsersListRoundButtonWidget_1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UsersListRoundButtonWidget_2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UsersListRoundButtonWidget_3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UsersListRoundButtonWidget_4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USizeBox* SizeBox_ExpandListButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UTooltipWidget> UsersListTooltipClass;

	UFUNCTION()
	void OnReleaseButton_ExpandUsersList();

	void UpdateHorizontalUsersList(const TArray<AEditModelPlayerState*>& InPlayerStates);
	TArray<class UUsersListButtonWidget*> GetUserListButtonsArray() const;
};
