// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeleteMenuWidget.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UDeleteMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDeleteMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:

	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	FGuid PresetGUIDToDelete;
	FGuid PresetGUIDReplacement;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UNCPNavigator* NCPNavigatorWidget;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonClose;

	void BuildDeleteMenu();
	void SetPresetToDelete(const FGuid& InPresetToDelete, const FGuid& InPresetToReplace);
	void BuildDeleteModalDialog();

	const FGuid& GetPresetGUIDToDelete() const { return PresetGUIDToDelete; }

	UFUNCTION()
	void ModalDeleteButtonConfirmReleased();

	UFUNCTION()
	void OnReleaseButtonClose();
};
