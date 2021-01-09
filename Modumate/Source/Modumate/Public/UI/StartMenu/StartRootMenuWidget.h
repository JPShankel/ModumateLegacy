// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "StartRootMenuWidget.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UStartRootMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStartRootMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	UPROPERTY()
	class UModumateGameInstance *ModumateGameInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName NewLevelName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString HelpURL;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonHelp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonQuit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UStartBlockHomeWidget *Start_Home_BP;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UHorizontalBox* OpenCreateNewButtonsBox;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonOpen;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* ButtonCreateNew;

	UPROPERTY(Transient)
	FUserWidgetPool UserWidgetPool;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UFUNCTION()
	void OnButtonReleasedQuit();

	UFUNCTION()
	void OnButtonReleasedHelp();

	UFUNCTION()
	void OnButtonReleasedOpen();

	UFUNCTION()
	void OnButtonReleasedCreateNew();

	void ShowStartMenu();
};
