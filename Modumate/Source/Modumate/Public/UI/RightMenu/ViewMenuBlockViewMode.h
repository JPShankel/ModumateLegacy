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
	class UModumateButtonUserWidget* Button_ObjectMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_SurfaceGraphMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (BindWidget))
	class UModumateButtonUserWidget* Button_MetaplaneMode;

	UFUNCTION()
	void UpdateEnabledEditModes(const TArray<EEditViewModes>& EnabledEditModes);

	UFUNCTION()
	void SetActiveEditMode(EEditViewModes EditMode);

protected:
	TMap<EEditViewModes, class UModumateButtonUserWidget*> EditModesToButtons;
};
