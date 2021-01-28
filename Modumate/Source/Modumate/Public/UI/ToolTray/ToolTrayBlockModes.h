// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"

#include "ToolTrayBlockModes.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UToolTrayBlockModes : public UUserWidget
{
	GENERATED_BODY()

public:
	UToolTrayBlockModes(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	TArray<class UModumateButtonUserWidget*> AllModumateButtons;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWrapBox* WrapBox_Buttons;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonLine;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonRectangle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonMPBucket;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonRoofPerimeter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonOpeningStamp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonOpeningSystem;

	UFUNCTION(BlueprintCallable)
	void ChangeToSeparatorToolsButtons(EToolMode mode);

	UFUNCTION(BlueprintCallable)
	void ChangeToAttachmentToolsButtons(EToolMode mode);

	void SetButtonsState(const TArray<UModumateButtonUserWidget*>& ButtonsToShow);
};
