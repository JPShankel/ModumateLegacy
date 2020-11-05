// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ViewMenuBlockViewMode.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UViewMenuBlockViewMode : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_MetaGraphMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_SeparatorsMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_SurfaceGraphsMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_AllObjectsMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_PhysicalMode;

	UFUNCTION()
	void UpdateEnabledViewModes(const TArray<EEditViewModes>& EnabledViewModes);

	UFUNCTION()
	void SetActiveViewMode(EEditViewModes ViewMode);

protected:
	TMap<EEditViewModes, class UModumateButtonUserWidget*> ViewModesToButtons;
};
