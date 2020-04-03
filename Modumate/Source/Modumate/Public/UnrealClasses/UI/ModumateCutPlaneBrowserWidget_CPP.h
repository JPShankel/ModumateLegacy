// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModumateCutPlaneBrowserWidget_CPP.generated.h"

/**
 *
 */


UCLASS()
class MODUMATE_API UModumateCutPlaneBrowserWidget_CPP : public UUserWidget
{
	GENERATED_BODY()


public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bListVerticalCutPlanesOnly;
};
