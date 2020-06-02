// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModumateCutPlaneBrowserWidget.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateCutPlaneBrowserWidget : public UUserWidget
{
	GENERATED_BODY()


public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bListVerticalCutPlanesOnly;
};
