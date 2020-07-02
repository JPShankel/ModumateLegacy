// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectAssembly.h"

#include "ComponentAssemblyListItem.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UComponentAssemblyListItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UComponentAssemblyListItem(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	FName AsmKey;
	class AEditModelPlayerController_CPP *EMPlayerController;
	EToolMode ToolMode;
	class UTextureRenderTarget2D *IconRenderTarget;

public:
	UPROPERTY()
	class UToolTrayBlockAssembliesList *ToolTrayBlockAssembliesList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlockUserWidget *MainText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButton *ModumateButtonMain;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget *ButtonEdit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *IconImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UVerticalBox *VerticalBoxProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UModumateTextBlockUserWidget> ModumateTextBlockUserWidgetClass;

	UFUNCTION()
	void OnModumateButtonMainReleased();

	UFUNCTION()
	void OnButtonEditReleased();

	bool BuildFromAssembly(AEditModelPlayerController_CPP *Controller, EToolMode mode, const FModumateObjectAssembly *Asm);
	bool CaptureIconRenderTarget();
	bool GetItemTips(TArray<FString> &OutTips);

};
