// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ViewMenuWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UViewMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UViewMenuWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY()
	class UComponentSavedViewListItem *CurrentHoverViewItem;

	UPROPERTY()
	class UTextureRenderTarget2D *PreviewRT;

	UPROPERTY()
	class AEditModelPlayerController_CPP *Controller;

	UPROPERTY()
	class AEditModelPlayerPawn_CPP *PlayerPawn;

	int32 HoverCaptureTickCount = 0;
	bool EnableHoverCapture = false;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint PreviewRenderSize = FIntPoint(480, 270);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NumberOfTickForHoverPreview = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UViewMenuBlockSavedViews *ViewMenu_Block_SavedViews;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UBorder *BorderPreview;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *ImagePreview;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage *ImageBG;

	void SetViewMenuVisibility(bool NewVisible);
	void MouseOnHoverView(UComponentSavedViewListItem *Item);
	void MouseEndHoverView(UComponentSavedViewListItem *Item);
	void HoverCaptureTick();
};
