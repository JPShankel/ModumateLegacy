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
	class UModumateButtonUserWidget* ButtonAxesNone;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonAxesXY;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonAxesZ;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonMPBucket;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonRoofPerimeter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonMetaPlaneLine;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonMetaPlaneHorizontal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonMetaPlaneVertical;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonOpeningStamp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonOpeningSystem;

	UFUNCTION(BlueprintCallable)
	void ChangeToMetaPlaneToolsButtons();

	UFUNCTION(BlueprintCallable)
	void ChangeToSurfaceGraphToolsButtons();

	UFUNCTION(BlueprintCallable)
	void ChangeToSeparatorToolsButtons(EToolMode mode);

	void SetButtonsState(const TArray<UModumateButtonUserWidget*>& ButtonsToShow);
};
