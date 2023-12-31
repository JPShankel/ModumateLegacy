// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Objects/CameraView.h"

#include "ComponentSavedViewListItem.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UComponentSavedViewListItem : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UComponentSavedViewListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController *Controller;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *ListNumber;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateEditableTextBoxUserWidget* SavedViewEdiableTitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonDelete;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonUpdate;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonEdit;

	// TODO: Prefer not button because this item needs to be drag-able
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ButtonMain;

	UFUNCTION()
	void OnButtonEditReleased();

	UFUNCTION()
	void OnButtonMainReleased();

	UFUNCTION()
	void OnButtonDeleteReleased();

	UFUNCTION()
	void OnButtonUpdateReleased();

	UFUNCTION()
	void OnEditableTitleCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void ActivateCameraView();

	FMOICameraViewData CameraView;

	// UserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
};
