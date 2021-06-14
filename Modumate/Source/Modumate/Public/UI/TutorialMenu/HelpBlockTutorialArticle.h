// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "HelpBlockTutorialArticle.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UHelpBlockTutorialArticle : public UUserWidget
{
	GENERATED_BODY()

public:
	UHelpBlockTutorialArticle(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY()
	class AEditModelPlayerController* EMPlayerController;

	FGuid CurrentTutorialGUID;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* ArticleSubtitle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget* ArticleMainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonGoBack;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UWebBrowser* WebBrowserVideo;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class USizeBox* SizeBoxVideo;

	void BuildTutorialArticle(const FGuid& InTutorialGUID);
	void ToggleWebBrowser(bool bEnable);

	UFUNCTION()
	void OnButtonGoBackPress();
};
