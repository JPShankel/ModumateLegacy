// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PresetCardItemObject.generated.h"


UCLASS(BlueprintType)
class MODUMATE_API UPresetCardItemObject : public UObject
{
	GENERATED_BODY()

public:
	FGuid PresetGuid;
};
