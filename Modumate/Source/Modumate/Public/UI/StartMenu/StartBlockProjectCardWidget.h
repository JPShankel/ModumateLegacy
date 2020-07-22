// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartBlockProjectCardWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UStartBlockProjectCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStartBlockProjectCardWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY()
	class AMainMenuGameMode_CPP *MainMenuGameMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButtonProjectCard;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *ImageScene;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock *TextBlockDate;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *TextBlockTitle;

	bool BuildProjectCard(int32 ProjectID);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnButtonReleasedProjectCard();

	FString ProjectPath;
};
