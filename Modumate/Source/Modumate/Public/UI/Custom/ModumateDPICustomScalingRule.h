// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DPICustomScalingRule.h"
#include "ModumateDPICustomScalingRule.generated.h"

/**
 *
 */
UCLASS()
class MODUMATE_API UModumateDPICustomScalingRule : public UDPICustomScalingRule
{
	GENERATED_BODY()

public:

	virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const override;

};
