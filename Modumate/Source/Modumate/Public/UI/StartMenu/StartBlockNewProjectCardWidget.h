// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartBlockNewProjectCardWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UStartBlockNewProjectCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStartBlockNewProjectCardWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButtonNewProject;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnButtonReleasedNewProject();
};
