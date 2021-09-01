// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/UsersList/UsersListButtonWidget.h"
#include "UI/Custom/TooltipWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ListView.h"

#include "UsersListVerticalEntryWidget.generated.h"

class AEditModelPlayerState;

UCLASS()
class MODUMATE_API UUsersListVerticalEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UUsersListVerticalEntryWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UserIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Username;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Email;
};
