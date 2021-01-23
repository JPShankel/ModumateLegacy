// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "BIMScopeWarning.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UBIMScopeWarning : public UUserWidget
{
	GENERATED_BODY()

public:
	UBIMScopeWarning(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	TArray<class UBIMScopeWarningList*> ListWidgets;

	UPROPERTY()
	TArray<class UBIMScopeWarningListItem*> ListItemWidgets;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonConfirm;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonDismiss;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox* VerticalBoxList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMScopeWarningList> BIMScopeWarningTypeListClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UBIMScopeWarningListItem> BIMScopeWarningTypeListItemClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FMargin ListItemPadding = FMargin(0.f, 6.f, 0.f, 0.f);

	UFUNCTION()
	void ConfirmScopeWarning();

	UFUNCTION()
	void DismissScopeWarning();

	void BuildScopeWarning(const TArray<FGuid>& InAffectedGuid);
};
