// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SplashBlockNewProjectCardWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API USplashBlockNewProjectCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USplashBlockNewProjectCardWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName NewLevelName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButtonNewProject;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnButtonReleasedNewProject();
};
