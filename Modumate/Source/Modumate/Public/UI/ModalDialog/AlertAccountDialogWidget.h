// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "AlertAccountDialogWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UAlertAccountDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAlertAccountDialogWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonInfoLink;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDismiss;

	UFUNCTION()
	void OnReleaseButtonInfoLink();

	UFUNCTION()
	void OnReleaseButtonDismiss();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ButtonInfoLinkURL;

protected:

};
