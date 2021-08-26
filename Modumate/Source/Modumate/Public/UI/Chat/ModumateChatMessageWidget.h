// Copyright © 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "Online/ModumateAccountManager.h"
#include "UI/Custom/ModumateTextBlockUserWidget.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UI/ModumateUIStatics.h"
#include "UI/UsersList/UsersListButtonWidget.h"
#include "Containers/Map.h"
#include "Blueprint/IUserObjectListEntry.h"

#include "ModumateChatMessageWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateChatMessageWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UModumateChatMessageWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UUsersListButtonWidget* UserIcon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Username;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* Timestamp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateSelectableTextBlock* MessageTextSelectable;

protected:
	virtual void NativeConstruct() override;

};
