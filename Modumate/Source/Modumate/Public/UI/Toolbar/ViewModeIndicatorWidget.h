// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Database/ModumateObjectEnums.h"

#include "ViewModeIndicatorWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UViewModeIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UViewModeIndicatorWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateTextBlock* ViewModeText;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UImage* ViewModeImage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconObjectEditing;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconMetaPlanes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	class UTexture2D* IconSurfaceGraphs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextObjectEditing;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextMetaPlanes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText TextSurfaceGraphs;

	void SwitchToViewMode(EEditViewModes NewViewMode);

};
