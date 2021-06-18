// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "HelpBlockTutorialMoreLinks.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UHelpBlockTutorialMoreLinks : public UUserWidget
{
	GENERATED_BODY()

public:
	UHelpBlockTutorialMoreLinks(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget* Button_Forum;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonIconTextUserWidget* Button_WhatsNew;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ForumURL;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString WhatsNewURL;

	UFUNCTION()
	void OnButtonReleasedForum();

	UFUNCTION()
	void OnButtonReleasedWhatsNew();
};
