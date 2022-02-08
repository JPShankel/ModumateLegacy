// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Objects/ModumateObjectInstance.h"

#include "MOIPattern2D.generated.h"

USTRUCT()
struct MODUMATE_API FMOIPattern2DData
{
	GENERATED_BODY()

	UPROPERTY()
	bool bPlaceholderData = false;
};

UCLASS()
class MODUMATE_API AMOIPattern2D : public AModumateObjectInstance
{
	GENERATED_BODY()

public:
	AMOIPattern2D();

	UPROPERTY()
	FMOIPattern2DData InstanceData;
};
