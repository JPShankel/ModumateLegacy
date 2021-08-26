// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Custom/ModumateButton.h"
#include "UI/Custom/ModumateCheckbox.h"
#include "UI/Custom/ModumateButtonUserWidget.h"
#include "UI/Custom/ModumateTextBlock.h"
#include "UI/EditModelUserWidget.h"


#include "Components/MultiLineEditableTextBox.h"

#include "ModumateChatEditableTextWidget.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UModumateChatEditableTextWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UModumateChatEditableTextWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY()
	class AEditModelPlayerController* Controller = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UModumateCheckBox* Checkbox_VoiceMicToggle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UModumateButton* Button_Submit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	UMultiLineEditableTextBox* TextBox_TextEntry;

	UFUNCTION()
	void OnMicToggled(bool bIsChecked);

	UFUNCTION()
	void OnSubmitReleased();

	UFUNCTION()
	void SubmitTextToChat();

	UFUNCTION()
	void OnTextChanged(const FText& InText);
};
